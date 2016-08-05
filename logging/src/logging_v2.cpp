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
#include <cocaine/idl/streaming.hpp>
#include <cocaine/rpc/slot.hpp>
#include <cocaine/repository/unicorn.hpp>
#include <cocaine/traits/vector.hpp>
#include <cocaine/unicorn/value.hpp>

#include <random>
#include <cocaine/detail/zookeeper/errors.hpp>

namespace ph = std::placeholders;
namespace bh = blackhole;

namespace cocaine {
namespace service {

namespace {

typedef api::unicorn_t::response response;

//TODO: move out to logging
bh::root_logger_t get_root_logger(context_t& context, const dynamic_t& service_args) {
    auto registry = blackhole::registry::configured();
    registry->add<blackhole::formatter::json_t>();

    std::stringstream stream;
    stream << boost::lexical_cast<std::string>(context.config().logging().loggers());

    return registry->builder<blackhole::config::json_t>(stream).build(
    service_args.as_object().at("backend", "core").as_string());
}

bool emit_ack(std::shared_ptr<logging::metafilter_t> filter,
              const std::string& backend,
              logging::logger_t& log,
              unsigned int severity,
              const std::string& message,
              const logging::attributes_t& attributes) {
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

void emit(std::shared_ptr<logging::metafilter_t> filter,
          const std::string& backend,
          logging::logger_t& log,
          unsigned int severity,
          const std::string& message,
          const logging::attributes_t& attributes) {
    emit_ack(filter, backend, log, severity, message, attributes);
}


}
struct logging_v2_t::impl_t {

    typedef logging::filter_t::deadline_t deadline_t;

    typedef std::tuple<std::string,
                       logging::filter_t::representation_t,
                       logging::filter_t::id_type,
                       logging::filter_t::disposition_t> filter_list_tuple_t;
    typedef std::vector<filter_list_tuple_t> filter_list_storage_t;

    static constexpr size_t retry_time_seconds = 5;
    static const std::string default_key;
    static const std::string core_key;

    impl_t(context_t& context, asio::io_service& io_context, bh::root_logger_t _logger, std::string _filter_unicorn_path)
        : internal_logger(context.log("logging_v2")),
          logger(std::move(_logger)),
          generator(std::random_device()()),
          unicorn(api::unicorn(context, "core")),
          filter_unicorn_path(std::move(_filter_unicorn_path)),
          list_scope(),
          retry_timer(io_context) {
        init();
    }

    void on_filter_list(std::future<response::children_subscribe> future) {
        try {
            auto filter_ids = future.get();
            COCAINE_LOG_DEBUG(internal_logger,
                              "received filter list update, count - {}",
                              std::get<1>(filter_ids).size());
            for (auto& id_str : std::get<1>(filter_ids)) {
                logging::filter_t::id_type filter_id = std::stoull(id_str);
                listen_for_filter(filter_id);
            }
        } catch (const std::system_error& e) {
            COCAINE_LOG_ERROR(internal_logger,
                              "failed to receive filter list update - {}",
                              error::to_string(e));
            retry_timer.async_wait([&](std::error_code){
                auto cb = std::bind(&impl_t::on_filter_list, this, ph::_1);
                unicorn->children_subscribe(std::move(cb), filter_unicorn_path);
            });
            retry_timer.expires_from_now(boost::posix_time::seconds(retry_time_seconds));
        }
    }

    void on_node_creation(unsigned long scope_id, std::future<bool> future) {
        try {
            future.get();
            auto cb = std::bind(&impl_t::on_filter_list, this, ph::_1);
            list_scope = unicorn->children_subscribe(std::move(cb), filter_unicorn_path);
            COCAINE_LOG_DEBUG(internal_logger, "created filter in unicorn");
        } catch (const std::system_error& e) {
            if(e.code().value() != ZNODEEXISTS) {
                retry_timer.expires_from_now(boost::posix_time::seconds(retry_time_seconds));
                retry_timer.async_wait([&](std::error_code) { init(); });
                COCAINE_LOG_ERROR(internal_logger,
                                  "failed to create filter in unicorn - {}",
                                  error::to_string(e));
            }
        }
        scopes.apply([&](std::unordered_map<size_t, api::unicorn_scope_ptr>& _scopes) {
            _scopes.erase(scope_id);
        });
    }

    void on_filter_creation(unsigned long scope_id,
                            unsigned int filter_id,
                            deferred<logging::filter_t::id_type> deferred,
                            std::future<bool> future) {
        try {
            future.get();
            deferred.write(filter_id);
            COCAINE_LOG_DEBUG(internal_logger, "created filter in unicorn");
        } catch (const std::system_error& e) {
            deferred.abort(e.code(), error::to_string(e));
            COCAINE_LOG_ERROR(internal_logger,
                              "failed to create filter {} in unicorn - {}",
                              filter_id,
                              error::to_string(e));
        }
        scopes.apply([&](std::unordered_map<size_t, api::unicorn_scope_ptr>& _scopes) {
            _scopes.erase(scope_id);
        });
    }

    void on_filter_recieve(unsigned long filter_id,
                           std::future<response::subscribe> filter_future) {
        try {
            auto filter_value = filter_future.get();
            COCAINE_LOG_DEBUG(internal_logger, "received filter update for id {} ", filter_id);
            if (filter_value.get_version() == unicorn::not_existing_version) {
                bool removed = metafilters.apply(
                [&](std::map<std::string, std::shared_ptr<logging::metafilter_t>>& mf) -> bool {
                    for (auto& mf_pair : mf) {
                        if(mf_pair.second->remove_filter(filter_id)) {
                            return true;
                        }
                    }
                    return false;
                });
                if(!removed) {
                    COCAINE_LOG_ERROR(internal_logger, "possible bug - removing unregistered filter {}", filter_id);
                }
            } else {
                logging::filter_info_t info(filter_value.get_value());
                auto metafilter = metafilters.apply(
                [&](std::map<std::string, std::shared_ptr<logging::metafilter_t>>& mf) {
                    auto& ret = mf[info.logger_name];
                    if (ret == nullptr) {
                        std::unique_ptr<logging::logger_t> mf_logger(new blackhole::wrapper_t(
                            *internal_logger, {{"metafilter", info.logger_name}}));
                            ret = std::make_shared<logging::metafilter_t>(std::move(mf_logger));
                    }
                    return ret;
                });
                metafilter->add_filter(std::move(info));
            }
        } catch (const std::system_error& e) {
            COCAINE_LOG_ERROR(internal_logger,
                              "can not create cluster filter - {}",
                              error::to_string(e));
        }
    }

    void init() {
        auto scope_id = scope_counter++;
        scopes.apply([&](std::unordered_map<size_t, api::unicorn_scope_ptr>& _scopes) {
            auto cb = std::bind(&impl_t::on_node_creation, this, scope_id, ph::_1);
            _scopes[scope_id] = unicorn->create(std::move(cb),
                                                filter_unicorn_path,
                                                unicorn::value_t(),
                                                false,
                                                false);
        });
    }

    std::vector<std::string> list_loggers() const {
        std::vector<std::string> result;
        metafilters.apply(
            [&](const std::map<std::string, std::shared_ptr<logging::metafilter_t>>& mf) {
                result.reserve(mf.size());
                for (auto& pair : mf) {
                    result.push_back(pair.first);
                }
            });
        return result;
    }

    deferred<logging::filter_t::id_type> set_filter(std::string name,
                                          logging::filter_t filter,
                                          uint64_t ttl,
                                          logging::filter_t::disposition_t disposition) {

        deferred<logging::filter_t::id_type> deferred;
        deadline_t deadline(logging::filter_t::clock_t::now() + std::chrono::seconds(ttl));
        uint64_t id = (*generator.synchronize())();
        logging::filter_info_t info(std::move(filter),
                                    std::move(deadline),
                                    id,
                                    disposition,
                                    std::move(name));

        if (disposition == logging::filter_t::disposition_t::local) {
            auto metafilter = get_metafilter(info.logger_name);
            metafilter->add_filter(std::move(info));
            deferred.write(id);
        } else if (disposition == logging::filter_t::disposition_t::cluster) {
            unsigned long scope_id = scope_counter++;
            auto cb = std::bind(&impl_t::on_filter_creation, this, scope_id, id, deferred, ph::_1);
            scopes.apply([&](std::unordered_map<size_t, api::unicorn_scope_ptr>& _scopes) {
                _scopes[scope_id] = unicorn->create(std::move(cb),
                                                    filter_unicorn_path + format("/%llu", id),
                                                    info.representation(),
                                                    false,
                                                    false);
            });
        } else {
            BOOST_ASSERT(false);
        }
        return deferred;
    }


    deferred<bool> remove_filter(logging::filter_t::id_type id) {
        deferred<bool> result;
        auto cb = [=](std::future<response::del> future) mutable {
            metafilters.apply([=](std::map<std::string, std::shared_ptr<logging::metafilter_t>>& mfs) mutable {
                for (auto& metafilter_pair : mfs) {
                    if (metafilter_pair.second->remove_filter(id)) {
                        result.write(true);
                        return;
                    }
                }
                result.write(false);
            });
            try {
                future.get();
            } catch (const std::system_error& e) {
                //TODO check code for nonode, retry
            }
        };
        auto path = filter_unicorn_path + format("/%llu", id);
        unicorn->del(std::move(cb), path, unicorn::not_existing_version);
        return result;
    }

    filter_list_storage_t list_filters() {
        return metafilters.apply(
            [&](std::map<std::string, std::shared_ptr<logging::metafilter_t>>& mfs) {
                filter_list_storage_t storage;

                auto callable = [&](const logging::filter_info_t& info) {
                    storage.push_back(std::make_tuple(
                    info.logger_name, info.filter.representation(), info.id, info.disposition));
                };
                for (auto& metafilter_pair : mfs) {
                    metafilter_pair.second->each(callable);
                }
                return storage;
            });
    }

    void listen_for_filter(logging::filter_t::id_type id) {
        unsigned long scope_id = scope_counter++;
        auto cb = std::bind(&impl_t::on_filter_recieve, this, scope_id, ph::_1);
        scopes.apply([&](std::unordered_map<size_t, api::unicorn_scope_ptr>& _scopes) {
            _scopes[scope_id] =
                unicorn->subscribe(std::move(cb), filter_unicorn_path + format("/%llu", id));
        });
    }


    std::shared_ptr<logging::metafilter_t> get_non_empty_metafilter(const std::string& name) {
        auto mf = get_metafilter(name);
        if(mf->empty()) {
            mf = get_metafilter(default_key);
        }
        return mf;
    }

    std::shared_ptr<logging::metafilter_t> get_default_metafilter() {
        return get_metafilter(default_key);
    }

    std::shared_ptr<logging::metafilter_t> get_core_metafilter() {
        return get_metafilter(core_key);
    }

    std::shared_ptr<logging::metafilter_t> get_metafilter(const std::string& name) {
        return metafilters.apply(
        [&](std::map<std::string, std::shared_ptr<logging::metafilter_t>>& _metafilters) {
            auto& metafilter = _metafilters[name];
            if (metafilter == nullptr) {
                std::unique_ptr<logging::logger_t> mf_logger(new blackhole::wrapper_t(
                *(internal_logger), {{"metafilter", name}}));
                metafilter = std::make_shared<logging::metafilter_t>(std::move(mf_logger));
            }
            return metafilter;
        });
    }

    std::unique_ptr<logging::logger_t> internal_logger;
    bh::root_logger_t logger;
    synchronized<std::map<std::string, std::shared_ptr<logging::metafilter_t>>> metafilters;
    synchronized<std::mt19937_64> generator;
    api::unicorn_ptr unicorn;
    std::atomic_ulong scope_counter;
    std::string filter_unicorn_path;
    synchronized<std::unordered_map<size_t, api::unicorn_scope_ptr>> scopes;
    api::unicorn_scope_ptr list_scope;
    asio::deadline_timer retry_timer;
};

const std::string logging_v2_t::impl_t::default_key("default");
const std::string logging_v2_t::impl_t::core_key("core");

class logging_v2_t::logging_slot_t : public io::basic_slot<io::base_log::get> {
public:
    typedef io::base_log::get event_type;
    typedef typename io::basic_slot<event_type>::dispatch_type dispatch_type;
    typedef typename io::basic_slot<event_type>::tuple_type tuple_type;
    // This one is type of upstream.
    typedef typename io::basic_slot<event_type>::upstream_type upstream_type;

    logging_slot_t(logging_v2_t& _parent) : parent(_parent) {}

    virtual boost::optional<std::shared_ptr<const dispatch_type>> operator()(const std::vector<hpack::header_t>&,
                                                                             tuple_type&& args,
                                                                             upstream_type&&) {
        return tuple::invoke(std::forward<tuple_type>(args), [&](std::string name) {
            auto metafilter = parent.impl->get_metafilter(name);
            std::shared_ptr<const dispatch_type> dispatch = std::make_shared<const named_logging_t>(
                parent.impl->logger, std::move(name), std::move(metafilter));
            return boost::make_optional(std::move(dispatch));
        });
    }

private:
    logging_v2_t& parent;
};

template<class Event>
class named_logging_t::emit_slot_t : public io::basic_slot<Event> {
public:
    typedef Event event_type;
    typedef io::basic_slot<event_type> super;
    typedef typename super::dispatch_type dispatch_type;
    typedef typename super::tuple_type tuple_type;
    typedef typename super::upstream_type upstream_type;

    typedef typename io::protocol<typename io::event_traits<event_type>::upstream_type>::scope protocol;
    //typedef typename io::aux::protocol_impl<typename io::event_traits<event_type>::upstream_type>::type protocol;



    emit_slot_t(named_logging_t& _parent, bool _need_ack) : parent(_parent), need_ack(_need_ack) {}

    virtual boost::optional<std::shared_ptr<const dispatch_type>> operator()(const std::vector<hpack::header_t>&,
                                                                             tuple_type&& args,
                                                                             upstream_type&& upstream) {
        bool result = emit_ack(parent.filter,
                               parent.backend,
                               parent.log,
                               std::get<0>(args),
                               std::get<1>(args),
                               std::get<2>(args));
        if(need_ack) {
            upstream.template send<typename protocol::chunk>(result);
        }

        return boost::none;
    }

private:
    named_logging_t& parent;
    bool need_ack;
};

void logging_v2_t::deleter_t::operator()(impl_t* ptr) {
    delete ptr;
}

const io::basic_dispatch_t& logging_v2_t::prototype() const {
    return *this;
}

logging_v2_t::logging_v2_t(context_t& context,
                     asio::io_service& asio,
                     const std::string& service_name,
                     const dynamic_t& service_args)
    : service_t(context, asio, service_name, service_args),
      dispatch<io::base_log_tag>(service_name)
{
    typedef logging::filter_t::disposition_t disposition_t;
    impl.reset(new impl_t(context,
                    asio,
                    get_root_logger(context, service_args),
                    service_args.as_object().at("unicorn_path", "/logging_v2/filters").as_string()));

    auto default_mf = impl->get_default_metafilter();
    auto default_metafilter_conf = service_args.as_object().at("default_metafilter").as_array();

    std::map<std::string, dynamic_t> filter_info_conf;

    // int64_t max value is used instead of uint64_t max value
    // to prevent overflow in std::duration, from which timepoint is constructed.
    filter_info_conf["deadline"] = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    filter_info_conf["logger_name"] = impl->default_key;
    for(const auto& filter_conf : default_metafilter_conf) {
        filter_info_conf["id"] = (*impl->generator.synchronize())();
        filter_info_conf["filter"] = filter_conf;
        default_mf->add_filter(logging::filter_info_t(filter_info_conf));
    }
    auto core_mf = impl->get_core_metafilter();
    filter_t core_filter([=](blackhole::severity_t sev, blackhole::attribute_pack& pack) {
        logging::filter_result_t result;
        if(core_mf->empty()) {
            result = default_mf->apply(sev, pack);
        } else {
            result = core_mf->apply(sev, pack);
        }
        if(result == logging::filter_result_t::accept) {
            return true;
        } else {
            return false;
        }
    });
    auto cp = core_filter;
    context.logger_filter(std::move(core_filter));
    context.logger_filter(std::move(cp));

    on<io::base_log::emit>([&](unsigned int severity,
                               const std::string& backend,
                               const std::string& message,
                               const logging::attributes_t& attributes){
        emit(impl->get_non_empty_metafilter(backend),
             backend,
             impl->logger,
             severity,
             message,
             attributes);
    });
    on<io::base_log::emit_ack>([&](unsigned int severity,
                                   const std::string& backend,
                                   const std::string& message,
                                   const logging::attributes_t& attributes) {
        return emit_ack(impl->get_non_empty_metafilter(backend),
                        backend,
                        impl->logger,
                        severity,
                        message,
                        attributes);
    });
    on<io::base_log::verbosity>([](){ return 0; });
    on<io::base_log::set_verbosity>([](unsigned int){});
    on<io::base_log::get>(std::make_shared<logging_slot_t>(*this));
    on<io::base_log::list_loggers>(std::bind(&impl_t::list_loggers, impl.get()));
    on<io::base_log::set_filter>(std::bind(&impl_t::set_filter,
                                           impl.get(),
                                           ph::_1,
                                           ph::_2,
                                           ph::_3,
                                           disposition_t::local));
    on<io::base_log::remove_filter>(std::bind(&impl_t::remove_filter, impl.get(), ph::_1));
    on<io::base_log::list_filters>(std::bind(&impl_t::list_filters, impl.get()));
    on<io::base_log::set_cluster_filter>(std::bind(&impl_t::set_filter,
                                                   impl.get(),
                                                   ph::_1,
                                                   ph::_2,
                                                   ph::_3,
                                                   disposition_t::cluster));
}

named_logging_t::named_logging_t(logging::logger_t& _log,
                                 std::string _name,
                                 std::shared_ptr<logging::metafilter_t> _filter) :
        dispatch<io::named_log_tag>(format("named_logging/{}", _name)),
        log(_log),
        backend(std::move(_name)),
        filter(std::move(_filter))
{
    on<io::named_log::emit>(std::make_shared<emit_slot_t<io::named_log::emit>>(*this, false));
    on<io::named_log::emit_ack>(std::make_shared<emit_slot_t<io::named_log::emit_ack>>(*this, true));
}

}
}  // namespace cocaine::service
