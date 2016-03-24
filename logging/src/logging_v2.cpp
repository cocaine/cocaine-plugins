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

#include "cocaine/traits/attribute.hpp"
#include "cocaine/traits/filter.hpp"

#include "cocaine/service/logging.hpp"

#include <blackhole/config/json.hpp>
#include <blackhole/registry.hpp>
#include <blackhole/root.hpp>
#include <blackhole/sink/console.hpp>
#include <blackhole/wrapper.hpp>

#include <cocaine/api/unicorn.hpp>
#include <cocaine/context.hpp>
#include <cocaine/rpc/slot.hpp>
#include <cocaine/traits/vector.hpp>
#include <cocaine/unicorn/value.hpp>

#include <random>

namespace ph = std::placeholders;
namespace bh = blackhole;

using namespace cocaine;

namespace cocaine {
namespace service {

namespace {
struct create_handler_t
    : public unicorn::writable_adapter_base_t<api::unicorn_t::response::create> {
    create_handler_t(logging_v2_t::impl_t& _parent, uint64_t _scope_id)
        : parent(_parent), scope_id(_scope_id) {}
    virtual void write(bool&&);

    virtual void abort(const std::error_code&, const std::string&);

    logging_v2_t::impl_t& parent;
    uint64_t scope_id;
};

struct filter_list_subscription_t
    : public unicorn::writable_adapter_base_t<api::unicorn_t::response::children_subscribe>,
      public std::enable_shared_from_this<filter_list_subscription_t> {
    filter_list_subscription_t(logging_v2_t::impl_t& _parent) : parent(_parent) {}

    virtual void write(
        std::tuple<cocaine::unicorn::version_t, std::vector<std::string>>&& filter_ids);

    virtual void abort(const std::error_code&, const std::string&);

    logging_v2_t::impl_t& parent;
};

struct id_subscription_t
    : public unicorn::writable_adapter_base_t<api::unicorn_t::response::subscribe> {
    id_subscription_t(logging_v2_t::impl_t& _parent, uint64_t _id) : parent(_parent), id(_id) {}

    virtual void write(api::unicorn_t::response::subscribe&& filter_value);

    virtual void abort(const std::error_code&, const std::string&);

    logging_v2_t::impl_t& parent;
    uint64_t id;
    std::string name;
};

struct list_filters_visitor_t : public logging::metafilter_t::visitor_t {
    typedef std::tuple<std::string,
                       logging::filter_t::representation_t,
                       logging::filter_t::id_type,
                       logging::filter_t::disposition_t>
        tuple_type;
    typedef std::vector<tuple_type> storage_type;
    storage_type storage;

    void operator()(const logging::filter_info_t& info) {
        storage.push_back(std::make_tuple(
            info.logger_name, info.filter.representation(), info.id, info.disposition));
    }
};
}
struct logging_v2_t::impl_t {
    impl_t(context_t& context, bh::root_logger_t _logger, std::string _filter_unicorn_path)
        : internal_logger(context.log("logging_v2")),
          logger(std::move(_logger)),
          generator(std::random_device()()),
          unicorn(api::unicorn(context, "core")),
          filter_unicorn_path(std::move(_filter_unicorn_path)),
          list_scope() {
        auto id = scope_counter++;
        scopes.unsafe()[id] = unicorn->create(std::make_shared<create_handler_t>(*this, id),
                                              filter_unicorn_path,
                                              unicorn::value_t(),
                                              false,
                                              false);
        list_scope = unicorn->children_subscribe(
            std::make_shared<filter_list_subscription_t>(*this), filter_unicorn_path);
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

    logging::filter_t::id_type set_filter(const std::string& name,
                                          logging::filter_t filter,
                                          uint64_t ttl,
                                          logging::filter_t::disposition_t disposition) {
        logging::filter_t::deadline_t deadline(std::chrono::steady_clock::now() +
                                               std::chrono::seconds(ttl));
        uint64_t id = (*generator.synchronize())();
        logging::filter_info_t info(
            std::move(filter), std::move(deadline), id, disposition, std::move(name));
        if (disposition == logging::filter_t::disposition_t::local) {
            auto metafilter = metafilters.apply(
                [&](std::map<std::string, std::shared_ptr<logging::metafilter_t>>& mf) {
                    auto& ret = mf[info.logger_name];
                    if (ret == nullptr) {
                        std::unique_ptr<logging::logger_t> mf_logger(
                            new blackhole::wrapper_t(*internal_logger, {{"metafilter", name}}));
                        ret = std::make_shared<logging::metafilter_t>(std::move(mf_logger));
                    }
                    return ret;
                });
            metafilter->add_filter(std::move(info));
        } else if (disposition == logging::filter_t::disposition_t::cluster) {
            unsigned long scope_id = scope_counter++;
            auto handler = std::make_shared<create_handler_t>(*this, scope_id);
            scopes.apply([&](std::unordered_map<size_t, api::unicorn_scope_ptr>& _scopes) {
                _scopes[scope_id] = unicorn->create(handler,
                                                    filter_unicorn_path + format("/%llu", id),
                                                    info.representation(),
                                                    false,
                                                    false);
            });
        } else {
            BOOST_ASSERT(false);
        }
        return id;
    }

    bool remove_filter(logging::filter_t::id_type id) {
        // TODO: Maybe it's better to store map from id to metafilter?
        return metafilters.apply(
            [&](std::map<std::string, std::shared_ptr<logging::metafilter_t>>& mfs) {
                for (auto& metafilter_pair : mfs) {
                    if (metafilter_pair.second->remove_filter(id)) {
                        return true;
                    }
                }
                return false;
            });
    }

    list_filters_visitor_t::storage_type list_filters() {
        return metafilters.apply(
            [&](std::map<std::string, std::shared_ptr<logging::metafilter_t>>& mfs) {
                list_filters_visitor_t visitor;
                for (auto& metafilter_pair : mfs) {
                    metafilter_pair.second->apply_visitor(visitor);
                }
                return visitor.storage;
            });
    }

    void listen_for_filter(logging::filter_t::id_type id) {
        unsigned long scope_id = scope_counter++;
        auto handler = std::make_shared<id_subscription_t>(*this, scope_id);
        scopes.apply([&](std::unordered_map<size_t, api::unicorn_scope_ptr>& _scopes) {
            _scopes[scope_id] =
                unicorn->subscribe(handler, filter_unicorn_path + format("/%llu", id));
        });
    }

    std::unique_ptr<logging::logger_t> internal_logger;
    bh::root_logger_t logger;

    synchronized<std::map<std::string, std::shared_ptr<logging::metafilter_t>>> metafilters;
    synchronized<std::mt19937_64> generator;
    api::category_traits<api::unicorn_t>::ptr_type unicorn;
    std::atomic_ulong scope_counter;
    std::string filter_unicorn_path;
    synchronized<std::unordered_map<size_t, api::unicorn_scope_ptr>> scopes;
    api::unicorn_scope_ptr list_scope;
};

void create_handler_t::write(bool&&) {
    COCAINE_LOG_DEBUG(parent.internal_logger, "created filter in unicorn");
    parent.scopes.apply([&](std::unordered_map<size_t, api::unicorn_scope_ptr>& _scopes) {
        _scopes.erase(scope_id);
    });
}

void create_handler_t::abort(const std::error_code& ec, const std::string& reason) {
    COCAINE_LOG_ERROR(parent.internal_logger,
                      "failed to create filter in unicorn, {} - {}",
                      reason,
                      ec.message());
    // TODO handle error somehow. Maybe requery?
    parent.scopes.apply([&](std::unordered_map<size_t, api::unicorn_scope_ptr>& _scopes) {
        _scopes.erase(scope_id);
    });
}

void filter_list_subscription_t::write(
    std::tuple<cocaine::unicorn::version_t, std::vector<std::string>>&& filter_ids) {
    COCAINE_LOG_DEBUG(parent.internal_logger,
                      "received filter list update, count - {}",
                      std::get<1>(filter_ids).size());
    for (auto& id_str : std::get<1>(filter_ids)) {
        logging::filter_t::id_type filter_id = std::stoull(id_str);
        parent.listen_for_filter(filter_id);
    }
}

void filter_list_subscription_t::abort(const std::error_code& ec, const std::string& error) {
    COCAINE_LOG_ERROR(parent.internal_logger,
                      "failed to receive filter list update, {} - {}",
                      error,
                      ec.message());
    // TODO : timer
    parent.unicorn->children_subscribe(shared_from_this(), parent.filter_unicorn_path);
}

void id_subscription_t::write(api::unicorn_t::response::subscribe&& filter_value) {
    COCAINE_LOG_DEBUG(parent.internal_logger, "received filter update for id {} ", id);
    try {
        if (filter_value.get_version() == unicorn::not_existing_version) {
            if (name.empty()) {
                throw error_t("possible bug - removing unregistered filter %llu", id);
            }
            auto metafilter = parent.metafilters.apply(
                [&](std::map<std::string, std::shared_ptr<logging::metafilter_t>>& mf) {
                    auto& ret = mf[name];
                    return ret;
                });
            if (metafilter) {
                metafilter->remove_filter(id);
            }
        } else {
            logging::filter_info_t info(filter_value.get_value());
            name = info.logger_name;
            auto metafilter = parent.metafilters.apply(
                [&](std::map<std::string, std::shared_ptr<logging::metafilter_t>>& mf) {
                    auto& ret = mf[name];
                    if (ret == nullptr) {
                        std::unique_ptr<logging::logger_t> internal_logger(new blackhole::wrapper_t(
                            *parent.internal_logger, {{"metafilter", info.logger_name}}));
                        ret = std::make_shared<logging::metafilter_t>(std::move(internal_logger));
                    }
                    return ret;
                });
            metafilter->add_filter(std::move(info));
        }
    } catch (const std::system_error& e) {
        COCAINE_LOG_ERROR(
            parent.internal_logger, "can not create cluster filter - %s", error::to_string(e));
    }
}

void id_subscription_t::abort(const std::error_code& ec, const std::string& error) {
    COCAINE_LOG_ERROR(
        parent.internal_logger, "failed to receive filter update, {} - {}", error, ec.message());
}

class logging_v2_t::logging_slot_t : public io::basic_slot<io::base_log::get> {
public:
    typedef io::base_log::get event_type;
    typedef typename io::basic_slot<event_type>::dispatch_type dispatch_type;
    typedef typename io::basic_slot<event_type>::tuple_type tuple_type;
    // This one is type of upstream.
    typedef typename io::basic_slot<event_type>::upstream_type upstream_type;

    logging_slot_t(logging_v2_t& _parent) : parent(_parent) {}

    virtual boost::optional<std::shared_ptr<const dispatch_type>> operator()(tuple_type&& args,
                                                                             upstream_type&&) {
        return tuple::invoke(std::forward<tuple_type>(args), [&](std::string name) {
            auto metafilter = parent.impl->metafilters.apply(
                [&](std::map<std::string, std::shared_ptr<logging::metafilter_t>>& metafilters) {
                    auto& mf = metafilters[name];
                    if (mf == nullptr) {
                        std::unique_ptr<logging::logger_t> internal_logger(new blackhole::wrapper_t(
                            *(parent.impl->internal_logger), {{"metafilter", name}}));
                        mf = std::make_shared<logging::metafilter_t>(std::move(internal_logger));
                    }
                    return mf;
                });
            std::shared_ptr<const dispatch_type> dispatch = std::make_shared<const named_logging_t>(
                parent.impl->logger, std::move(name), std::move(metafilter));
            return boost::make_optional(std::move(dispatch));
        });
    }

private:
    logging_v2_t& parent;
};

struct dynamic_visitor_t {
    void operator()(const dynamic_t::array_t&) {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument),
                                "can not process array as attribute value");
    }

    void operator()(const dynamic_t::object_t&) {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument),
                                "can not process map as attribute value");
    }

    void operator()(dynamic_t::null_t) {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument),
                                "can not process null as attribute value");
    }

    template <class T>
    void operator()(const T& value) {
        attributes.emplace_back(blackhole::attribute_t(name, value));
    }

    blackhole::attributes_t& attributes;
    const std::string& name;
    typedef void result_type;
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
      dispatch<io::base_log_tag>(service_name),
      impl(new impl_t(
          context,
          [&]() {
              auto registry = blackhole::registry_t::configured();
              registry.add<blackhole::sink::console_t>();

              std::stringstream stream;
              stream << boost::lexical_cast<std::string>(context.config.logging.loggers);

              return registry.builder<blackhole::config::json_t>(stream).build(
                  service_args.as_object().at("backend", "core").as_string());
          }(),
          service_args.as_object().at("unicorn_path", "/logging_v2/filters").as_string())) {
    on<io::base_log::get>(std::make_shared<logging_slot_t>(*this));
    on<io::base_log::list_loggers>(std::bind(&impl_t::list_loggers, impl.get()));
    on<io::base_log::set_filter>(std::bind(&impl_t::set_filter,
                                           impl.get(),
                                           ph::_1,
                                           ph::_2,
                                           ph::_3,
                                           logging::filter_t::disposition_t::local));
    on<io::base_log::remove_filter>(std::bind(&impl_t::remove_filter, impl.get(), ph::_1));
    on<io::base_log::list_filters>(std::bind(&impl_t::list_filters, impl.get()));
    on<io::base_log::set_cluster_filter>(std::bind(&impl_t::set_filter,
                                                   impl.get(),
                                                   ph::_1,
                                                   ph::_2,
                                                   ph::_3,
                                                   logging::filter_t::disposition_t::cluster));
}

named_logging_t::named_logging_t(logging::logger_t& _log,
                                 std::string _name,
                                 std::shared_ptr<logging::metafilter_t> _filter)
    : dispatch<io::named_log_tag>(std::move(_name)), log(_log), filter(std::move(_filter)) {
    on<io::named_log::emit>(std::bind(&named_logging_t::emit, this, ph::_1, ph::_2, ph::_3));
    on<io::named_log::emit_ack>(
        std::bind(&named_logging_t::emit_ack, this, ph::_1, ph::_2, ph::_3));
}

void named_logging_t::emit(const std::string& message,
                           unsigned int severity,
                           const logging::attributes_t& attributes) {
    emit_ack(message, severity, attributes);
}

bool named_logging_t::emit_ack(const std::string& message,
                               unsigned int severity,
                               const logging::attributes_t& attributes) {
    if (filter->apply(message, severity, attributes) == logging::filter_result_t::reject) {
        return false;
    }
    if (!attributes.empty()) {
        blackhole::attributes_t bh_attributes;
        for (const auto& attribute : attributes) {
            dynamic_visitor_t visitor{bh_attributes, attribute.name};
            attribute.value.apply(visitor);
        }
        blackhole::attribute_list attribute_list(bh_attributes.begin(), bh_attributes.end());
        blackhole::attribute_pack attribute_pack({attribute_list});
        log.log(blackhole::severity_t(severity), message, attribute_pack);
    } else {
        log.log(blackhole::severity_t(severity), message);
    }
    return true;
}
}
}  // namespace cocaine::service
