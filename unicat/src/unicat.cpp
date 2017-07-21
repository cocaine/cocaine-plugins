#include <functional>
#include <memory>
#include <map>
#include <type_traits>
#include <tuple>
#include <vector>

#include <blackhole/logger.hpp>
#include <blackhole/scope/holder.hpp>
#include <blackhole/wrapper.hpp>

#include <cocaine/api/authentication.hpp>
#include <cocaine/context.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/format.hpp>
#include <cocaine/format/vector.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/traits/optional.hpp>

#include "cocaine/service/unicat/unicat.hpp"
#include "cocaine/idl/unicat.hpp"

#include "backend/backend.hpp"
#include "backend/fabric.hpp"

#include "auth_cache.hpp"
#include "completion.hpp"

// #define LOCAL_DEBUG
#undef LOCAL_DEBUG

namespace cocaine {
namespace service {

namespace cu = cocaine::unicat;

using completion_state_ptr = std::shared_ptr<cu::base_completion_state_t>;
using identity_ptr = std::shared_ptr<auth::identity_t>;

namespace detail {

    const auto DEFAULT_SERVICE_NAME = std::string{"core"};

    using entities_by_scheme_type = std::map<
        cu::scheme_t,
        std::map<
            std::string,                // service, set by user or defaults to 'scheme'
            std::vector<std::string>    // entities
        >>;

    auto
    separate_by_scheme(const std::vector<cu::entity_type>& entities) -> entities_by_scheme_type
    {
        std::string scheme;
        std::string entity;
        boost::optional<std::string> service;

        entities_by_scheme_type separated;

        for(const auto& el : entities) {
            std::tie(scheme, service, entity) = el;

            auto& service_mapping = separated[cu::scheme_from_string(scheme)];
            if (service && service->empty() == false) {
                service_mapping[*service].push_back(entity);;
            } else {
                service_mapping[DEFAULT_SERVICE_NAME].push_back(entity);
            }
        }

        return separated;
    }

    template<typename Deferred, typename Logger>
    auto
    abort_deferred(
        Deferred& deferred,
        std::shared_ptr<Logger> log,
        const std::error_code ec,
        const cu::completion_t::Opcode opcode,
        const cu::url_t url,
        identity_ptr& identity,
        const std::string& message) -> void {

        COCAINE_LOG_WARNING(log, "access error: {} {}, {}",
            ec.value(), ec.message(), message);

        deferred.set_completion(cu::completion_t{url, identity, ec, opcode});
    }

    template<typename R>
    auto
    unpack(const unicorn::versioned_value_t value) -> std::tuple<R, cu::version_t> {
        if (value.exists() && value.value().convertible_to<R>()) {
            return std::make_tuple(value.value().to<R>(), value.version());
        }
        // Such error code make sense only in unicat context, should be less
        // specific in general case.
        throw std::system_error(make_error_code(error::invalid_acl_framing));
    }

    template<typename Completions>
    auto wait_all(Completions&& completions) -> std::vector<std::exception_ptr> {
        std::vector<std::exception_ptr> errors;
        for(auto& fut : completions) {
            try {
                fut.get();
            } catch(...) {
                errors.push_back(std::current_exception());
            }
        }
        return errors;
    }
} // detail

namespace dbg {
    auto
    log_requested_entities(std::shared_ptr<logging::logger_t> log, const std::vector<cu::entity_type>& entities)
        -> void
    {
        for(const auto& ent : entities) {
            const auto& scheme = std::get<0>(ent);
            const auto& service = std::get<1>(ent);
            const auto& entity = std::get<2>(ent);

            COCAINE_LOG_DEBUG(log, "alter slot for {}::{}::{}",
                scheme, service && service->empty() == false ? *service : detail::DEFAULT_SERVICE_NAME,
                entity);
        }
    }
} // dbg

struct on_write_t : unicat::async::write_handler_t {
    const cu::url_t url;
    identity_ptr identity;
    completion_state_ptr completion_state;

    on_write_t(const cu::url_t url, identity_ptr identity, completion_state_ptr completion_state) :
        url(std::move(url)),
        identity(std::move(identity)),
        completion_state(std::move(completion_state))
    {}

    virtual auto on_write(std::future<void> fut) -> void override {
        on_done(std::move(fut));
    }

    virtual auto on_write(std::future<api::unicorn_t::response::put> fut) -> void override {
        on_done(std::move(fut));
    }

private:
    template<typename R>
    auto on_done(std::future<R> fut) -> void {
        try {
            fut.get();
            completion_state->set_completion(cu::completion_t{url, identity});
        } catch(...) {
            // Can also throw!
            completion_state->set_completion(cu::completion_t{url, identity, std::current_exception()});
        }
    }
};

template<typename Event>
struct on_read_t :
    public unicat::async::read_handler_t
{
    std::shared_ptr<cu::backend_t> backend;
    const cu::url_t url;
    identity_ptr identity;
    auth::alter_data_t alter_data;
    completion_state_ptr completion_state;

    on_read_t(
        std::shared_ptr<cu::backend_t> backend,
        const cu::url_t url,
        const identity_ptr identity,
        auth::alter_data_t alter_data,
        completion_state_ptr completion_state) :
            backend(std::move(backend)),
            url(std::move(url)),
            identity(std::move(identity)),
            alter_data(std::move(alter_data)),
            completion_state(std::move(completion_state))
    {}

    ~on_read_t() {}

    auto on_read(std::future<unicorn::versioned_value_t> fut) -> void override {
        try {
            auto data = fut.get();

            auto meta = auth::metainfo_t{};
            auto version = cu::version_t{};

            std::tie(meta, version) = detail::unpack<auth::metainfo_t>(data);
            on_read(meta, version);
        } catch(...) {
            on_exception(std::current_exception());
        }
    }

    auto on_read(std::future<auth::metainfo_t> fut) -> void override {
        try {
            on_read(fut.get());
        } catch(...) {
            on_exception(std::current_exception());
        }
    }
private:
    auto on_read(auth::metainfo_t metainfo, const cu::version_t version = cocaine::unicorn::not_existing_version) -> void {
        auth::alter<Event>(metainfo, alter_data);

        backend->async_write_metainfo(url.entity, version, metainfo,
            std::make_shared<on_write_t>(url, identity, completion_state));
    }

    auto on_exception(std::exception_ptr eptr) -> void {
        backend.reset();
        completion_state->set_completion(cu::completion_t{url, identity, std::move(eptr)});
    }
};

template<typename Event>
struct alter_slot_t :
    public io::basic_slot<Event>
{
    using tuple_type = typename io::basic_slot<Event>::tuple_type;
    using upstream_type = typename io::basic_slot<Event>::upstream_type;
    using result_type = typename io::basic_slot<Event>::result_type;

    using protocol = typename io::aux::protocol_impl<typename io::event_traits<Event>::upstream_type>::type;

    context_t& context;

    std::shared_ptr<cu::authorization::handlers_cache_t> auth_cache;
    std::shared_ptr<logging::logger_t> log;

    alter_slot_t(context_t& context, const std::string& name, std::shared_ptr<cu::authorization::handlers_cache_t> auth_cache) :
        context(context),
        auth_cache(auth_cache),
        log(context.log("audit", {{"service", name}}))
    {}

    auto
    operator()(const std::vector<hpack::header_t>& headers, tuple_type&& args, upstream_type&& upstream)
        -> result_type
    try {
        const auto& entities = std::get<0>(args);
        const auto& cids = std::get<1>(args);
        const auto& uids = std::get<2>(args);
        const auto& flags = std::get<3>(args);

        // dbg::log_requested_entities(log, entities);

        COCAINE_LOG_INFO(log, "alter slot with cids {} and uids {} set flags {}", cids, uids, flags);

        const auto alter_data = auth::alter_data_t{cids, uids, flags};

        auto completion_state =
            std::make_shared<cu::async_completion_state_t<upstream_type, protocol>>(
                upstream,
                context.log("audit", {{"service", "unicat"}}),
                Event::alias());

        for(const auto& it: detail::separate_by_scheme(entities)) {
            const auto& scheme = it.first;
            const auto& services = it.second;

            for(const auto& srv : services) {
                const auto& name = srv.first;
                const auto& entities = srv.second;

                COCAINE_LOG_INFO(log, "alter metainfo for scheme {} and service {}",
                    cu::scheme_to_string(scheme), name);

#if !defined(LOCAL_DEBUG)
                auto auth = api::authentication(context, "core", name);
                auto identity = std::make_shared<auth::identity_t>(auth->identify(headers));
#else // for local debug
                auto identity = std::make_shared<auth::identity_t>(
                    // auth::identity_t::builder_t().cids({1,13}).uids({4,6}).build());
                    auth::identity_t::builder_t().cids({1}).uids({3}).build());
                    // auth::identity_t::builder_t().cids({100}).uids({300}).build());
#endif

                auto backend = cu::fabric::make_backend(scheme,
                    cu::backend_t::options_t{
                        context, name, context.log("audit", {{"service", name}}), auth_cache} );

                for(const auto entity : entities) {
                    auto url = cu::url_t{scheme, name, entity};

                    auto on_verify_read = cu::async::verify_handler_t{
                        identity,
                        [=] (std::error_code ec) mutable -> void {
                            if (ec) {
                                return detail::abort_deferred(
                                    *completion_state,
                                    backend->logger(),
                                    ec,
                                    cu::completion_t::Opcode::ReadOp,
                                    url, identity,
                                    "'read' access error");
                            }

                            auto on_verify_write = cu::async::verify_handler_t{
                                identity,
                                [=] (std::error_code ec) mutable -> void {
                                    if (ec) {
                                        return detail::abort_deferred(
                                            *completion_state,
                                            backend->logger(),
                                            ec, cu::completion_t::Opcode::WriteOp,
                                            url, identity,
                                            "'write' access error");
                                    }

                                    auto on_read = std::make_shared<on_read_t<Event>>(
                                        backend,
                                        url,
                                        identity,
                                        alter_data,
                                        completion_state);

                                    backend->async_read_metainfo(entity, std::move(on_read));
                                }
                            }; // on_verify_write{}

                            backend->async_verify_read(entity, on_verify_write);
                        }
                    }; // on_verify_read{}

                    backend->async_verify_read(entity, on_verify_read);
                } // for entities
            } // for services
        } // for schemes

        COCAINE_LOG_DEBUG(log, "ACL altering request completed");
        return boost::none;
    } catch(const std::system_error& err) {
       COCAINE_LOG_WARNING(log, "failed to complete '{}' operation", Event::alias(), blackhole::attribute_list{
           {"code", err.code().value()},
           {"error", error::to_string(err)},
       });

       upstream.template send<typename protocol::error>(err.code(), error::to_string(err));
       return boost::none;
   } catch(const std::exception& err) {
        COCAINE_LOG_WARNING(log, "failed to complete '{}' operation", Event::alias(), blackhole::attribute_list{
            {"error", err.what()},
        });
        upstream.template send<typename protocol::error>(error::uncaught_error, err.what());
        return boost::none;
   }
};

struct bind {
    template<typename Event, typename Self, typename... Args>
    static
    auto on_alter(Self&& self, Args&&... args) -> void {
        self.template on<Event>(std::make_shared<alter_slot_t<Event>>(std::forward<Args>(args)...));
    }
};

unicat_t::unicat_t(context_t& context, asio::io_service& asio, const std::string& service_name, const dynamic_t& args) :
    service_t(context, asio, service_name, args),
    dispatch<io::unicat_tag>(service_name)
{
    // TODO: looks redundant, refactor out.
    auto auth_cache = std::make_shared<cu::authorization::handlers_cache_t>(context);

    bind::on_alter<io::unicat::grant>(*this, context, service_name, auth_cache);
    bind::on_alter<io::unicat::revoke>(*this, context, service_name, auth_cache);
}

}  // namespace service
}  // namespace cocaine
