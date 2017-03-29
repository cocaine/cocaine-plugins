/*
* 2015+ Copyright (c) Anton Matveenko <antmat@yandex-team.ru>
* All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*/

#include "cocaine/service/unicorn.hpp"

#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/mpl/at.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/map.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include <blackhole/logger.hpp>
#include <blackhole/wrapper.hpp>

#include <cocaine/api/authentication.hpp>
#include <cocaine/api/authorization/unicorn.hpp>
#include <cocaine/context.hpp>
#include <cocaine/format/vector.hpp>
#include <cocaine/logging.hpp>

#include "cocaine/traits/unicorn.hpp"

using namespace cocaine::unicorn;

namespace cocaine { namespace service {

template<class Event, class Method, class Response>
class unicorn_slot_t :
    public io::basic_slot<Event>
{
public:
    typedef typename result_of<Event>::type result_type;
    typedef typename io::basic_slot<Event>::dispatch_type dispatch_type;
    typedef typename io::basic_slot<Event>::tuple_type tuple_type;
    // This one is type of upstream.
    typedef typename io::basic_slot<Event>::upstream_type upstream_type;
    // And here we use only upstream tag.
    typedef typename io::aux::protocol_impl<typename io::event_traits<Event>::upstream_type>::type protocol;

    unicorn_slot_t(const unicorn_service_t& service_,
                   std::shared_ptr<api::unicorn_t> unicorn_,
                   Method method_,
                   std::shared_ptr<api::authentication_t> auth_,
                   std::shared_ptr<api::authorization::unicorn_t> authorization_) :
        service(service_),
        unicorn(unicorn_),
        method(method_),
        auth(std::move(auth_)),
        authorization(std::move(authorization_))
    {}

    virtual
    boost::optional<std::shared_ptr<dispatch_type>>
    operator()(const std::vector<hpack::header_t>& headers, tuple_type&& args, upstream_type&& upstream) {
        typedef boost::optional<std::shared_ptr<dispatch_type>> result_dispatch_type;

        const auto& path = extract_path(args);
        if (!is_allowed(path)) {
            upstream.template send<typename protocol::error>(error::invalid_path, "root path is not allowed");
            return result_dispatch_type(std::make_shared<unicorn_dispatch_t>(service.name()));
        }

        auth::identity_t identity;
        try {
            identity = auth->identify(headers);
        } catch (const std::system_error& err) {
            COCAINE_LOG_WARNING(service.log, "failed to complete '{}' operation", Event::alias(), blackhole::attribute_list{
                {"code", err.code().value()},
                {"error", err.code().message()},
                {"reason", error::to_string(err)},
            });

            upstream.template send<typename protocol::error>(err.code(), error::to_string(err));
            return result_dispatch_type(std::make_shared<unicorn_dispatch_t>(service.name()));
        }

        const auto& ident = boost::get<auth::identity_t>(identity);
        auto dispatch = std::make_shared<unicorn_dispatch_t>(service.name());

        auto log = std::make_shared<blackhole::wrapper_t>(*service.log, blackhole::attributes_t{
            {"event", std::string(Event::alias())},
            {"path", path},
            {"cids", cocaine::format("{}", ident.cids())},
            {"uids", cocaine::format("{}", ident.uids())},
        });

        Response response;

        auto on_response = [=](std::future<result_type> future) mutable {
            try {
                auto result = future.get();
                COCAINE_LOG_INFO(log, "completed '{}' operation", Event::alias());

                response.write(std::move(result));
            } catch (const std::system_error& err) {
                COCAINE_LOG_WARNING(log, "failed to complete '{}' operation", Event::alias(), blackhole::attribute_list{
                    {"code", err.code().value()},
                    {"error", error::to_string(err)}
                });

                try {
                    response.abort(err.code(), error::to_string(err));
                } catch(const std::system_error&) {
                    // Eat.
                }
            }
        };

        auto on_validated = [=](std::error_code ec) mutable {
            if (ec) {
                COCAINE_LOG_WARNING(log, "failed to complete '{}' operation", Event::alias(), blackhole::attribute_list{
                    {"code", ec.value()},
                    {"error", ec.message()},
                });
                upstream.template send<typename protocol::error>(ec, ec.message());
                return;
            }

            auto tuple = std::tuple_cat(std::make_tuple(unicorn.get(), on_response), std::move(args));

            try {
                dispatch->attach(tuple::invoke(std::move(tuple), std::mem_fn(method)));
            } catch(const std::system_error& err) {
                COCAINE_LOG_WARNING(log, "failed to complete '{}' operation", Event::alias(), blackhole::attribute_list{
                    {"code", err.code().value()},
                    {"error", error::to_string(err)},
                });

                upstream.template send<typename protocol::error>(err.code(), error::to_string(err));
            } catch(const std::exception& err) {
                COCAINE_LOG_WARNING(log, "failed to complete '{}' operation", Event::alias(), blackhole::attribute_list{
                    {"error", err.what()},
                });
                upstream.template send<typename protocol::error>(error::uncaught_error, err.what());
            }

            response.attach(std::move(upstream));
        };

        authorization->verify<Event>(path, ident, [=](std::error_code code) mutable {
            try {
                on_validated(code);
            } catch (const std::system_error& err) {
                // Only client-is-detached exceptions may be caught here.
            }
        });

        return result_dispatch_type(dispatch);
    }

private:
    static
    auto
    is_allowed(const path_t& path) -> bool {
        return path != "/";
    }

    static
    auto
    extract_path(const tuple_type& args) -> const std::string& {
        return std::get<0>(args);
    }

private:
    const unicorn_service_t& service;
    std::shared_ptr<api::unicorn_t> unicorn;
    Method method;

    std::shared_ptr<api::authentication_t> auth;
    std::shared_ptr<api::authorization::unicorn_t> authorization;
};

namespace {

template<typename Event>
struct response_of {
    typedef deferred<typename result_of<Event>::type> type;
};

template<>
struct response_of<io::unicorn::subscribe> {
    typedef streamed<typename result_of<io::unicorn::subscribe>::type> type;
};

template<>
struct response_of<io::unicorn::children_subscribe> {
    typedef streamed<typename result_of<io::unicorn::children_subscribe>::type> type;
};

template<typename Event>
struct method_of {
    typedef boost::mpl::map<
        boost::mpl::pair<io::unicorn::subscribe, decltype(&api::unicorn_t::subscribe)>,
        boost::mpl::pair<io::unicorn::children_subscribe, decltype(&api::unicorn_t::children_subscribe)>,
        boost::mpl::pair<io::unicorn::put, decltype(&api::unicorn_t::put)>,
        boost::mpl::pair<io::unicorn::get, decltype(&api::unicorn_t::get)>,
        boost::mpl::pair<io::unicorn::create, decltype(&api::unicorn_t::create_default)>,
        boost::mpl::pair<io::unicorn::del, decltype(&api::unicorn_t::del)>,
        boost::mpl::pair<io::unicorn::remove, decltype(&api::unicorn_t::del)>,
        boost::mpl::pair<io::unicorn::increment, decltype(&api::unicorn_t::increment)>,
        boost::mpl::pair<io::unicorn::lock, decltype(&api::unicorn_t::lock)>
    >::type mapping;

    typedef typename boost::mpl::at<mapping, Event>::type type;
};

template<typename Event>
using slot_of = unicorn_slot_t<
    Event,
    typename method_of<Event>::type,
    typename response_of<Event>::type
>;

struct register_slot_t {
    unicorn_service_t& dispatch;
    std::shared_ptr<api::unicorn_t> unicorn;
    std::shared_ptr<api::authentication_t> auth;
    std::shared_ptr<api::authorization::unicorn_t> authorization;

    template<typename T, typename Event>
    auto
    on(Event event) -> void {
        dispatch.on<T>(
            std::make_shared<slot_of<T>>(
                dispatch,
                unicorn,
                std::forward<Event>(event),
                auth,
                authorization
            )
        );
    }
};

} // namespace

unicorn_service_t::unicorn_service_t(context_t& context, asio::io_service& asio_, const std::string& name_, const dynamic_t& args) :
    service_t(context, asio_, name_, args),
    dispatch<io::unicorn_tag>(name_),
    log(context.log("audit", {{"service", name()}})),
    unicorn(api::unicorn(context, args.as_object().at("backend", "core").as_string()))
{
    typedef io::unicorn scope;

    auto auth = api::authentication(context, "core", name());
    auto authorization = api::authorization::unicorn(context, name());

    register_slot_t r{*this, unicorn, auth, authorization};

    r.on<scope::subscribe>(&api::unicorn_t::subscribe);
    r.on<scope::children_subscribe>(&api::unicorn_t::children_subscribe);
    r.on<scope::put>(&api::unicorn_t::put);
    r.on<scope::get>(&api::unicorn_t::get);
    r.on<scope::create>(&api::unicorn_t::create_default);
    r.on<scope::del>(&api::unicorn_t::del);
    r.on<scope::remove>(&api::unicorn_t::del);
    r.on<scope::increment>(&api::unicorn_t::increment);
    r.on<scope::lock>(&api::unicorn_t::lock);
}

unicorn_dispatch_t::unicorn_dispatch_t(const std::string& name_) :
    dispatch<io::unicorn_final_tag>(name_),
    state(state_t::initial)
{
    on<io::unicorn::close>([&] {
        discard(std::error_code());
    });
}

auto
unicorn_dispatch_t::attach(api::unicorn_scope_ptr nscope) -> void {
    BOOST_ASSERT(nscope != nullptr);
    BOOST_ASSERT(this->scope == nullptr);

    std::lock_guard<std::mutex> lock(mutex);
    this->scope = std::move(nscope);
    if (state == state_t::discarded) {
        this->scope->close();
    }
}

void
unicorn_dispatch_t::discard(const std::error_code& /* ec */) {
    std::lock_guard<std::mutex> lock(mutex);
    state = state_t::discarded;
    if (scope) {
        scope->close();
    }
}

}} //namespace cocaine::service
