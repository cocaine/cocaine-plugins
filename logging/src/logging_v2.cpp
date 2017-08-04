/*
    Copyright (c) 2011-2015 Anton Matveenko <antmat@yandex-team.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "cocaine/logging/metafilter.hpp"
#include "cocaine/logging/filter.hpp"

#include "cocaine/traits/attributes.hpp"
#include "cocaine/traits/dynamic.hpp"
#include "cocaine/traits/filter.hpp"

#include "cocaine/service/logging.hpp"

#include <blackhole/config/json.hpp>
#include <blackhole/formatter/json.hpp>
#include <blackhole/registry.hpp>
#include <blackhole/root.hpp>
#include <blackhole/sink/console.hpp>
#include <blackhole/sink/file.hpp>
#include <blackhole/sink/socket/tcp.hpp>
#include <blackhole/sink/socket/udp.hpp>
#include <blackhole/wrapper.hpp>

#include <cocaine/api/unicorn.hpp>
#include <cocaine/context.hpp>
#include <cocaine/context/config.hpp>
#include <cocaine/context/filter.hpp>
#include <cocaine/context/signal.hpp>
#include <cocaine/idl/context.hpp>
#include <cocaine/idl/streaming.hpp>
#include <cocaine/rpc/slot.hpp>
#include <cocaine/repository/unicorn.hpp>
#include <cocaine/trace/logger.hpp>
#include <cocaine/traits/vector.hpp>
#include <cocaine/unicorn/value.hpp>

#include "../foreign/radix_tree/radix_tree.hpp"

#include <random>

namespace ph = std::placeholders;
namespace bh = blackhole;

using namespace cocaine::logging;

namespace cocaine {
namespace service {

namespace {

using response = api::unicorn_t::response;

//TODO: move out to logging
auto get_root_logger(context_t& context, const dynamic_t& service_args) -> bh::root_logger_t {
    auto registry = blackhole::registry::configured();

    std::stringstream stream;
    stream << boost::lexical_cast<std::string>(context.config().logging().loggers());

    auto backend = service_args.as_object().at("backend", "core").as_string();
    return registry->builder<blackhole::config::json_t>(stream).build(backend);
}

auto emit_ack(std::shared_ptr<metafilter_t> filter, const std::string& backend, logger_t& log, unsigned int severity,
              const std::string& message, const attributes_t& attributes) -> bool
{
    blackhole::attribute_list attribute_list;
    for (const auto& attribute : attributes) {
        attribute_list.emplace_back(attribute);
    }
    attribute_list.emplace_back("source", backend);

    blackhole::attribute_pack attribute_pack({attribute_list});
    if (filter->apply(severity, attribute_pack) == logging::filter_result_t::reject) {
        return false;
    }
    log.log(blackhole::severity_t(severity), message, attribute_pack);
    return true;
}

auto emit(std::shared_ptr<metafilter_t> filter, const std::string& backend, logging::logger_t& log,
          unsigned int severity, const std::string& message, const logging::attributes_t& attributes) -> void
{
    emit_ack(filter, backend, log, severity, message, attributes);
}

auto now() -> uint64_t {
    return static_cast<uint64_t>(std::time(nullptr));
}
}

struct logging_v2_t::impl_t : public std::enable_shared_from_this<logging_v2_t::impl_t> {
    using filter_t = logging::filter_t;

    // propagate filter_t typedefs for simplicity
    using id_t = filter_t::id_t;
    using disposition_t = filter_t::disposition_t;
    using deadline_t = filter_t::deadline_t;
    using representation_t = filter_t::representation_t;

    using filter_list_tuple_t = std::tuple<std::string, representation_t, id_t, uint64_t, disposition_t>;
    using filter_list_storage_t = std::vector<filter_list_tuple_t>;

    static constexpr size_t retry_time_seconds = 5;
    static const std::string default_key;
    static const std::string core_key;

    impl_t(context_t& _context, asio::io_service& io_context, const dynamic_t& config) :
        context(_context),
        internal_logger(context.log("logging_v2")),
        root_logger(new bh::root_logger_t(get_root_logger(context, config))),
        logger(std::unique_ptr<logging::logger_t>(new bh::wrapper_t(*root_logger, {}))),
        signal_dispatcher(std::make_shared<dispatch<io::context_tag>>("logging_signals")),
        generator(std::random_device()()),
        unicorn(api::unicorn(context, "core")),
        filter_unicorn_path(config.as_object().at("unicorn_path", "/cocaine/logging_v2/filters").as_string()),
        retry_timer(io_context),
        cleanup_timer(io_context)
    {
        auto default_mf = get_default_metafilter();
        auto default_metafilter_conf = config.as_object().at("default_metafilter").as_array();

        std::map<std::string, dynamic_t> filter_info_conf;

        // int64_t max value is used instead of uint64_t max value
        // to prevent overflow in std::duration, from which timepoint is constructed.
        filter_info_conf["deadline"] = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
        filter_info_conf["logger_name"] = default_key;
        filter_info_conf["disposition"] = static_cast<uint64_t>(disposition_t::local);
        for(const auto& filter_conf : default_metafilter_conf) {
            filter_info_conf["id"] = generate_id();
            filter_info_conf["filter"] = filter_conf;
            default_mf->add_filter(logging::filter_info_t(filter_info_conf));
        }

        auto core_mf = get_core_metafilter();
        cocaine::filter_t core_filter([=](blackhole::severity_t sev, blackhole::attribute_pack& pack) {
            logging::filter_result_t result;
            if(core_mf->empty()) {
                result = default_mf->apply(sev, pack);
            } else {
                result = core_mf->apply(sev, pack);
            }
            return result == logging::filter_result_t::accept;
        });
        context.logger_filter(std::move(core_filter));

        signal_dispatcher->on<io::context::os_signal>([=](int signum, siginfo_t){
            if(signum == SIGHUP) {
                COCAINE_LOG_INFO(internal_logger, "resetting logging v2 root logger");
                *root_logger = get_root_logger(context, config);
            }
        });
        context.signal_hub().listen(signal_dispatcher, io_context);
        cleanup({});
    }

    template <class T, class F>
    struct
    safe_t {
        safe_t(std::weak_ptr<T> _self, F _f):
            self(std::move(_self)),
            f(std::move(_f))
        {}

        template<class... Args>
        auto operator()(Args&&... args) -> void {
            if(auto lock = self.lock()) {
                f(std::forward<Args>(args)...);
            }
        }

        std::weak_ptr<T> self;
        F f;
    };

    template<class T, class F>
    static auto safe_impl(std::weak_ptr<T> self, F&& f) -> safe_t<T, F> {
        return safe_t<T, F>(std::move(self), std::forward<F>(f));
    }

    template<class F>
    auto safe(F&& f) -> safe_t<impl_t, F> {
        return safe_impl(std::weak_ptr<impl_t>(shared_from_this()), std::forward<F>(f));
    }

    auto cleanup(const std::error_code& ec) -> void {
        if(!ec) {
            COCAINE_LOG_DEBUG(internal_logger, "cleaning up metafilters");
            metafilters.apply([&](metafilters_t& metafilters){
                // TODO: I have no idea how to iterative cleanup this concrete implementation of radix_tree,
                // so we use simple but slow implementation of empty metafilters cleanup
                std::vector<std::string> empty;
                for(auto& mf_pair: metafilters) {
                    mf_pair.second->cleanup();
                    // NOTE: core metafilter is stored by shared_ptr in filtering lambda and it is a special case
                    // We can introduce something like 'need_cleanup' or 'useless' method to metafilter,
                    // but for now it looks like an overkill, so we just check in cleanup if the name is 'core'
                    if(mf_pair.second->empty() && mf_pair.first != core_key){
                        empty.push_back(mf_pair.first);
                    }
                }
                for(auto& empty_item: empty) {
                    metafilters.erase(empty_item);
                }
            });
            cleanup_timer.expires_from_now(boost::posix_time::seconds(1));
            cleanup_timer.async_wait([&](const std::error_code& ec){ cleanup(ec);});
        }
    }

    auto load_filters() -> void {

        // callback to handle subscription on node with filter data
        auto on_filter = safe([=](id_t filter_id, std::future<response::subscribe> filter_future) {
            auto remove = [=]() {
                if(remove_local_filter(filter_id)) {
                    COCAINE_LOG_INFO(internal_logger, "removed filter {} from metafilter", filter_id);
                }
                // subscription ended - remove scope
                scopes->erase(filter_id);
            };

            try {
                auto filter_value = filter_future.get();
                COCAINE_LOG_DEBUG(internal_logger, "received filter update for id {} ", filter_id);

                if(!filter_value.exists()) {
                    remove();
                } else {
                    try {
                        logging::filter_info_t info(filter_value.value());
                        if(info.deadline < now()) {
                            COCAINE_LOG_INFO(internal_logger, "deadlined filter found - removing filter from unicorn");
                            remove_from_unicorn(filter_path(filter_id));
                        } else {
                            auto metafilter = get_metafilter(info.logger_name);
                            auto mf_name = info.logger_name;
                            metafilter->add_filter(std::move(info));
                        }
                    } catch (const std::exception& e) {
                        COCAINE_LOG_ERROR(internal_logger, "can not parse filter value, erasing filter {} from unicorn - {}",
                                          filter_id, e.what());
                        remove_from_unicorn(filter_path(filter_id));
                    }
                }
            } catch (const std::system_error& e) {
                if(e.code().value() != error::no_node) {
                    COCAINE_LOG_ERROR(internal_logger, "can not fetch cluster filter - {}", error::to_string(e));
                }
                remove();
            }
        });

        auto on_filter_list = safe([=](std::future<response::children_subscribe> future) {
            try {
                auto filter_ids = std::get<1>(future.get());
                COCAINE_LOG_DEBUG(internal_logger, "received filter list update, count - {}", filter_ids.size());
                for(auto& id_str : filter_ids) {
                    uint64_t filter_id;
                    try {
                        filter_id = std::stoull(id_str);
                    } catch (const std::exception& e) {
                        COCAINE_LOG_ERROR(internal_logger, "invalid filter key {} - {}, removing", id_str, e.what());
                        remove_from_unicorn(filter_unicorn_path + "/" + id_str);
                        continue;
                    }
                    auto cb = std::bind(on_filter, filter_id, ph::_1);
                    scopes.apply([&](unicorn_scopes_t& _scopes) {
                        auto& scope = _scopes[filter_id];
                        if(!scope) {
                            scope = unicorn->subscribe(std::move(cb), filter_path(filter_id));
                        }
                    });
                }
            } catch (const std::exception& e) {
                COCAINE_LOG_ERROR(internal_logger, "failed to receive filter list update - {}", e.what());
                retry_timer.async_wait([&](std::error_code) {
                    load_filters();
                });
                retry_timer.expires_from_now(boost::posix_time::seconds(retry_time_seconds));
            }
        });

        auto on_create = safe([=](std::future<bool> result) {
            try {
                result.get();
                COCAINE_LOG_DEBUG(internal_logger, "created filter path in unicorn");
            } catch (const std::system_error& e) {
                if(e.code().value() != error::node_exists) {
                    COCAINE_LOG_ERROR(internal_logger, "failed to create filter in unicorn - {}", error::to_string(e));
                    retry_timer.async_wait([&](std::error_code ec) {
                        if(!ec) {
                            load_filters();
                        }
                    });
                    retry_timer.expires_from_now(boost::posix_time::seconds(retry_time_seconds));
                    return;
                } else {
                    COCAINE_LOG_DEBUG(internal_logger, "filter path is already there");
                }
            }

            list_scope = unicorn->children_subscribe(on_filter_list, filter_unicorn_path);
        });

        scopes->clear();
        create_scope = unicorn->create(std::move(on_create), filter_unicorn_path, unicorn::value_t(), false, false);
    }


    auto remove_from_unicorn(std::string filter_path) -> void {
        unicorn->del(safe([=](std::future<response::del> future) {
            try {
                future.get();
                COCAINE_LOG_INFO(internal_logger, "removed filter from unicorn from {}", filter_path);
            } catch (const std::system_error& e) {
                if(e.code().value() != error::no_node) {
                    COCAINE_LOG_WARNING(internal_logger, "failed to remove filter on {} from ZK - {}",
                                        filter_path, error::to_string(e));
                } else {
                    COCAINE_LOG_DEBUG(internal_logger, "filter {} not found in unicorn", filter_path);
                }
            }
        }), filter_path, unicorn::not_existing_version);
    }

    auto filter_path(id_t id) -> std::string {
        return filter_unicorn_path + format("/{}", id);
    }

    auto list_loggers() -> std::vector<std::string> {
        std::vector<std::string> result;
        metafilters.apply([&](metafilters_t& mf) {
            result.reserve(mf.size());
            for (auto& pair : mf) {
                result.push_back(pair.first);
            }
        });
        return result;
    }

    auto generate_id() const -> id_t {
        return (*generator.synchronize())();
    }

    auto set_filter(std::string name, filter_t filter, uint64_t ttl, disposition_t disposition) -> deferred<id_t> {
        deferred<id_t> deferred;
        deadline_t deadline = now() + ttl;
        id_t id = generate_id();
        logging::filter_info_t info(std::move(filter), std::move(deadline), id, disposition, std::move(name));

        if (disposition == logging::filter_t::disposition_t::local) {
            auto metafilter = get_metafilter(info.logger_name);
            metafilter->add_filter(std::move(info));
            deferred.write(id);
        } else if (disposition == logging::filter_t::disposition_t::cluster) {
            unsigned long scope_id = scope_counter++;
            auto on_create = safe([=](std::future<bool> future) mutable {
                try {
                    future.get();
                    deferred.write(id);
                    COCAINE_LOG_DEBUG(internal_logger, "created filter in unicorn");
                } catch (const std::system_error& e) {
                    auto msg = format("failed to create filter {} in unicorn - {}", id, error::to_string(e));
                    deferred.abort(e.code(), msg);
                    COCAINE_LOG_ERROR(internal_logger, msg);
                }
                scopes.apply([&](unicorn_scopes_t& _scopes) {
                    _scopes.erase(scope_id);
                });
            });
            scopes.apply([&](std::unordered_map<size_t, api::unicorn_scope_ptr>& _scopes) {
                _scopes[scope_id] = unicorn->create(on_create, filter_path(id), info.representation(), false, false);
            });
        } else {
            BOOST_ASSERT(false);
        }
        return deferred;
    }

    auto remove_filter(id_t id) -> deferred<bool> {
        deferred<bool> result;
        auto path = filter_path(id);
        unicorn->del(safe([=](std::future<response::del> future) mutable {
            try {
                future.get();
                remove_local_filter(id);
                result.write(true);
                COCAINE_LOG_INFO(internal_logger, "removed filter from unicorn from {}", path);
            } catch (const std::system_error& e) {
                if(e.code().value() != error::no_node) {
                    result.abort(e.code(), "failed to remove filter from unicorn");
                    COCAINE_LOG_WARNING(internal_logger, "failed to remove filter on {} from ZK - {}", path, error::to_string(e));
                } else {
                    result.write(remove_local_filter(id));
                    COCAINE_LOG_DEBUG(internal_logger, "filter {} not found in unicorn", path);
                }
            }
        }), path, unicorn::not_existing_version);
        return result;
    }

    auto remove_local_filter(id_t id) -> bool {
        return metafilters.apply([=](metafilters_t& mfs) mutable {
            for(auto& metafilter_pair : mfs) {
                if(metafilter_pair.second->remove_filter(id)) {
                    return true;
                }
            }
            return false;
        });
    }

    auto list_filters() -> filter_list_storage_t {
        return metafilters.apply([&](metafilters_t& mfs) {
            filter_list_storage_t storage;

            auto callable = [&](const logging::filter_info_t& info) {
                storage.push_back(std::make_tuple(info.logger_name, info.filter.representation(), info.id, info.deadline, info.disposition));
            };
            for (auto& metafilter_pair : mfs) {
                metafilter_pair.second->each(callable);
            }
            return storage;
        });
    }

    auto find_metafilter(const std::string& name) -> std::shared_ptr<logging::metafilter_t> {
        auto mf = metafilters.apply([&](metafilters_t& _metafilters) -> std::shared_ptr<logging::metafilter_t> {
            COCAINE_LOG_DEBUG(internal_logger, "looking up longest match for {}", name);
            auto it = _metafilters.longest_match(name);
            if(it == _metafilters.end() || it->second->empty()) {
                return nullptr;
            } else {
                return it->second;
            }
        });
        if(mf) {
            return mf;
        } else {
            return get_default_metafilter();
        }
    }

    auto get_default_metafilter() -> std::shared_ptr<logging::metafilter_t> {
        return get_metafilter(default_key);
    }

    auto get_core_metafilter() -> std::shared_ptr<logging::metafilter_t>{
        return get_metafilter(core_key);
    }

    auto get_metafilter(const std::string& name) -> std::shared_ptr<logging::metafilter_t> {
        return metafilters.apply([&](metafilters_t& _metafilters) {
            auto& metafilter = _metafilters[name];
            if (metafilter == nullptr) {
                std::unique_ptr<logging::logger_t> mf_logger(new blackhole::wrapper_t(
                *(internal_logger), {{"metafilter", name}}));
                metafilter = std::make_shared<logging::metafilter_t>(context, name, std::move(mf_logger));
            }
            return metafilter;
        });
    }

    context_t& context;
    std::unique_ptr<logging::logger_t> internal_logger;
    std::unique_ptr<bh::root_logger_t> root_logger;
    logging::trace_wrapper_t logger;
    std::shared_ptr<dispatch<io::context_tag>> signal_dispatcher;

    using metafilters_t = radix_tree<std::string, std::shared_ptr<logging::metafilter_t>>;
    synchronized<metafilters_t> metafilters;
    mutable synchronized<std::mt19937_64> generator;
    api::unicorn_ptr unicorn;
    std::atomic_ulong scope_counter;
    std::string filter_unicorn_path;

    using unicorn_scopes_t = std::unordered_map<size_t, api::unicorn_scope_ptr>;
    api::unicorn_scope_ptr list_scope;
    api::unicorn_scope_ptr create_scope;
    synchronized<unicorn_scopes_t> scopes;
    asio::deadline_timer retry_timer;
    asio::deadline_timer cleanup_timer;
};

const std::string logging_v2_t::impl_t::default_key("default");
const std::string logging_v2_t::impl_t::core_key("core");

io::basic_dispatch_t& logging_v2_t::prototype() {
    return *this;
}

logging_v2_t::logging_v2_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& conf) :
        service_t(context, asio, name, conf),
        dispatch<io::base_log_tag>(name)
{
    d = std::make_shared<impl_t>(context, asio, conf);
    d->load_filters();

    on<io::base_log::emit>([&](uint severity, const std::string& backend, const std::string& message,
                               const attributes_t& attributes)
    {
        emit(d->find_metafilter(backend), backend, d->logger, severity, message, attributes);
    });

    on<io::base_log::emit_ack>([&](uint severity, const std::string& backend, const std::string& message,
                                   const attributes_t& attributes)
    {
        return emit_ack(d->find_metafilter(backend), backend, d->logger, severity, message, attributes);
    });

    using get = io::base_log::get;
    using get_slot_t = io::basic_slot<get>;
    on<get>([&](const hpack::headers_t&, get_slot_t::tuple_type&& args, get_slot_t::upstream_type&&){
        auto mf_name = std::get<0>(args);
        auto metafilter = d->find_metafilter(mf_name);
        auto dispatch = std::make_shared<named_logging_t>(d->logger, std::move(mf_name), std::move(metafilter));
        return get_slot_t::result_type(std::move(dispatch));
    });

    on<io::base_log::verbosity>([](){ return 0; });
    on<io::base_log::set_verbosity>([](unsigned int){});
    on<io::base_log::list_loggers>(std::bind(&impl_t::list_loggers, d.get()));

    using disposition_t = logging::filter_t::disposition_t;
    on<io::base_log::set_filter>(std::bind(&impl_t::set_filter, d.get(), ph::_1, ph::_2, ph::_3, disposition_t::local));
    on<io::base_log::set_cluster_filter>(std::bind(&impl_t::set_filter, d.get(), ph::_1, ph::_2, ph::_3, disposition_t::cluster));
    on<io::base_log::remove_filter>(std::bind(&impl_t::remove_filter, d.get(), ph::_1));
    on<io::base_log::list_filters>(std::bind(&impl_t::list_filters, d.get()));
}

named_logging_t::named_logging_t(logging::logger_t& _log,
                                 std::string _name,
                                 std::shared_ptr<logging::metafilter_t> _filter) :
        dispatch<io::named_log_tag>(format("named_logging/{}", _name)),
        log(_log),
        backend(std::move(_name)),
        filter(std::move(_filter))
{
    using emit_event = io::named_log::emit;
    using emit_slot_t = io::basic_slot<emit_event>;
    on<emit_event>([&](const hpack::headers_t&, emit_slot_t::tuple_type&& args, emit_slot_t::upstream_type&&) {
        auto severity = std::get<0>(args);
        auto& message = std::get<1>(args);
        auto& attributes = std::get<2>(args);
        emit(filter, backend, log, severity, message, attributes);
        return emit_slot_t::result_type(boost::none);
    });

    using ack_event = io::named_log::emit_ack;
    using ack_slot_t = io::basic_slot<ack_event>;

    on<ack_event>([&](const hpack::headers_t&, ack_slot_t::tuple_type&& args, ack_slot_t::upstream_type&& upstream) {
        auto severity = std::get<0>(args);
        auto& message = std::get<1>(args);
        auto& attributes = std::get<2>(args);
        auto result = emit_ack(filter, backend, log, severity, message, attributes);
        using chunk_event = io::protocol<io::stream_of<bool>::tag>::scope::chunk;
        upstream.template send<chunk_event>(result);
        return ack_slot_t::result_type(boost::none);
    });
}

}
}  // namespace cocaine::service
