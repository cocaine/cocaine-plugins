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

#include "cocaine/unicorn.hpp"
#include <cocaine/context.hpp>

using namespace cocaine::unicorn;

namespace cocaine { namespace service {

template<class Event, class Method, class Response>
class unicorn_slot_t :
    public io::basic_slot<Event>
{
public:
    typedef typename io::basic_slot<Event>::dispatch_type dispatch_type;
    typedef typename io::basic_slot<Event>::tuple_type tuple_type;
    typedef typename io::basic_slot<Event>::upstream_type upstream_type;

    unicorn_slot_t(unicorn_service_t* _service, std::shared_ptr<api::unicorn_t> _unicorn, Method _method) :
        service(_service),
        unicorn(_unicorn),
        method(_method)
    {}

    virtual
    boost::optional<std::shared_ptr<const dispatch_type>>
    operator()(tuple_type&& args, upstream_type&& upstream)
    {
        Response result;
        auto wptr = unicorn::make_writable(result);
        auto api_args = std::tuple_cat(std::make_tuple(unicorn.get(), wptr), std::move(args));
        auto request_scope = tuple::invoke(std::move(api_args), std::mem_fn(method));
        auto dispatch = std::make_shared<unicorn_dispatch_t>(service->get_name(), service, std::move(request_scope));
        result.attach(std::move(upstream));
        return boost::make_optional<std::shared_ptr<const dispatch_type>>(dispatch);
    }

private:
    unicorn_service_t* service;
    std::shared_ptr<api::unicorn_t> unicorn;
    Method method;
};

/**
* Typedefs for result type. Actual result types are in include/cocaine/idl/unicorn.hpp
*/
struct resp {
    typedef deferred<result_of<io::unicorn::put>::type> put;
    typedef deferred<result_of<io::unicorn::get>::type> get;
    typedef deferred<result_of<io::unicorn::create>::type> create;
    typedef deferred<result_of<io::unicorn::del>::type> del;
    typedef deferred<result_of<io::unicorn::increment>::type> increment;
    typedef streamed<result_of<io::unicorn::subscribe>::type> subscribe;
    typedef streamed<result_of<io::unicorn::children_subscribe>::type> children_subscribe;
    typedef deferred<result_of<io::unicorn::lock>::type> lock;
};

struct method {
    typedef decltype(&api::unicorn_t::children_subscribe) children_subscribe;
    typedef decltype(&api::unicorn_t::subscribe)          subscribe;
    typedef decltype(&api::unicorn_t::put)                put;
    typedef decltype(&api::unicorn_t::get)                get;
    typedef decltype(&api::unicorn_t::create_default)     create_default;
    typedef decltype(&api::unicorn_t::del)                del;
    typedef decltype(&api::unicorn_t::increment)          increment;
    typedef decltype(&api::unicorn_t::lock)               lock;
};

typedef io::unicorn scope;

typedef unicorn_slot_t<scope::children_subscribe, method::children_subscribe, resp::children_subscribe> children_subscribe_slot_t;
typedef unicorn_slot_t<scope::subscribe, method::subscribe,      resp::subscribe> subscribe_slot_t;
typedef unicorn_slot_t<scope::put,       method::put,            resp::put      > put_slot_t;
typedef unicorn_slot_t<scope::get,       method::get,            resp::get      > get_slot_t;
typedef unicorn_slot_t<scope::create,    method::create_default, resp::create   > create_slot_t;
typedef unicorn_slot_t<scope::del,       method::del,            resp::del      > del_slot_t;
typedef unicorn_slot_t<scope::increment, method::increment,      resp::increment> increment_slot_t;
typedef unicorn_slot_t<scope::lock,      method::lock,           resp::lock     > lock_slot_t;


const io::basic_dispatch_t&
unicorn_service_t::prototype() const {
    return *this;
}

unicorn_service_t::unicorn_service_t(context_t& context, asio::io_service& _asio, const std::string& _name, const dynamic_t& args) :
    service_t(context, _asio, _name, args),
    dispatch<io::unicorn_tag>(_name),
    name(_name),
    unicorn(api::unicorn(context, args.as_object().at("backend").as_string())),
    log(context.log("unicorn"))
{
    on<scope::subscribe>         (std::make_shared<subscribe_slot_t>         (this, unicorn, &api::unicorn_t::subscribe));
    on<scope::children_subscribe>(std::make_shared<children_subscribe_slot_t>(this, unicorn, &api::unicorn_t::children_subscribe));
    on<scope::put>               (std::make_shared<put_slot_t>               (this, unicorn, &api::unicorn_t::put));
    on<scope::get>               (std::make_shared<get_slot_t>               (this, unicorn, &api::unicorn_t::get));
    on<scope::create>            (std::make_shared<create_slot_t>            (this, unicorn, &api::unicorn_t::create_default));
    on<scope::del>               (std::make_shared<del_slot_t>               (this, unicorn, &api::unicorn_t::del));
    on<scope::increment>         (std::make_shared<increment_slot_t>         (this, unicorn, &api::unicorn_t::increment));
    on<scope::lock>              (std::make_shared<lock_slot_t>              (this, unicorn, &api::unicorn_t::lock));
}

unicorn_dispatch_t::unicorn_dispatch_t(const std::string& _name, unicorn_service_t* /*service*/, api::unicorn_scope_ptr _scope) :
    dispatch<io::unicorn_final_tag>(_name),
    scope(std::move(_scope))
{
    on<io::unicorn::close>(std::bind(&unicorn_dispatch_t::discard, this, std::error_code()));
}

void
unicorn_dispatch_t::discard(const std::error_code& /*ec*/) const {
    scope->close();
}

}} //namespace cocaine::service
