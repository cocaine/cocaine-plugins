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

#ifndef COCAINE_UNICORN_SERVICE_HPP
#define COCAINE_UNICORN_SERVICE_HPP

#include "cocaine/idl/unicorn.hpp"
#include "cocaine/unicorn/api/zookeeper.hpp"

#include "cocaine/zookeeper/connection.hpp"
#include "cocaine/zookeeper/session.hpp"

#include <cocaine/api/service.hpp>
#include <cocaine/rpc/dispatch.hpp>

namespace cocaine { namespace service {

class unicorn_service_t:
    public api::service_t,
    public dispatch<io::unicorn_tag>
{
public:
    virtual
    const io::basic_dispatch_t&
    prototype() const;

    unicorn_service_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args);
    friend class unicorn_dispatch_t;
    friend class distributed_lock_t;
    const std::string& get_name() const { return name; }
private:
    std::string name;
    zookeeper::session_t zk_session;
    zookeeper::connection_t zk;
    std::shared_ptr<cocaine::logging::log_t> log;
};

/**
* Service for providing access to unified configuration service.
* Currently we use zookeeper as backend.
* For protocol methods description see include/cocaine/idl/unicorn.hpp
*/
class unicorn_dispatch_t :
    public dispatch<io::unicorn_final_tag>
{
public:

    /**
    * Typedefs for result type. Actual result types are in include/cocaine/idl/unicorn.hpp
    */
    struct response {
        typedef deferred<result_of<io::unicorn::put>::type> put;
        typedef deferred<result_of<io::unicorn::get>::type> get;
        typedef deferred<result_of<io::unicorn::create>::type> create;
        typedef deferred<result_of<io::unicorn::del>::type> del;
        typedef deferred<result_of<io::unicorn::increment>::type> increment;
        typedef streamed<result_of<io::unicorn::subscribe>::type> subscribe;
        typedef streamed<result_of<io::unicorn::children_subscribe>::type> children_subscribe;
        typedef deferred<result_of<io::unicorn::lock>::type> lock;
    };

    unicorn_dispatch_t(const std::string& name, unicorn_service_t* parent);

    template<class Event, class Method, class Response>
    friend class unicorn_slot_t;

    virtual
    void discard(const std::error_code& ec) const;
private:

    //because discard is marked const
    mutable unicorn::zookeeper_api_t api;
};

template<class Event, class Method, class Response>
class unicorn_slot_t :
    public io::basic_slot<Event>
{
public:
    typedef typename io::basic_slot<Event>::dispatch_type dispatch_type;
    typedef typename io::basic_slot<Event>::tuple_type tuple_type;
    typedef typename io::basic_slot<Event>::upstream_type upstream_type;

    unicorn_slot_t(unicorn_service_t* _service, Method _method) :
        service(_service),
        method(_method)
    {}

    virtual
    boost::optional<std::shared_ptr<const dispatch_type>>
    operator()(tuple_type&& args, upstream_type&& upstream)
    {
        std::shared_ptr<unicorn_dispatch_t> dispatch = std::make_shared<unicorn_dispatch_t>(service->get_name(), service);
        Response result;
        auto wptr = unicorn::make_writable(result);
        auto callback = std::tuple_cat(std::make_tuple(&dispatch->api, wptr), std::move(args));
        tuple::invoke(std::mem_fn(method), std::move(callback));
        result.attach(std::move(upstream));
        return boost::make_optional<std::shared_ptr<const dispatch_type>>(dispatch);
    }

private:
    unicorn_service_t* service;
    Method method;
};

typedef unicorn_slot_t<io::unicorn::children_subscribe, decltype(&unicorn::zookeeper_api_t::children_subscribe), unicorn_dispatch_t::response::children_subscribe> children_subscribe_slot_t;
typedef unicorn_slot_t<io::unicorn::subscribe,  decltype(&unicorn::api_t::subscribe),  unicorn_dispatch_t::response::subscribe> subscribe_slot_t;
typedef unicorn_slot_t<io::unicorn::put,        decltype(&unicorn::api_t::put),        unicorn_dispatch_t::response::put> put_slot_t;
typedef unicorn_slot_t<io::unicorn::get,        decltype(&unicorn::api_t::get),        unicorn_dispatch_t::response::get> get_slot_t;
typedef unicorn_slot_t<io::unicorn::create,     decltype(&unicorn::api_t::create_default),     unicorn_dispatch_t::response::create> create_slot_t;
typedef unicorn_slot_t<io::unicorn::del,        decltype(&unicorn::api_t::del),        unicorn_dispatch_t::response::del> del_slot_t;
typedef unicorn_slot_t<io::unicorn::increment,  decltype(&unicorn::api_t::increment),  unicorn_dispatch_t::response::increment> increment_slot_t;
typedef unicorn_slot_t<io::unicorn::lock,       decltype(&unicorn::api_t::lock),       unicorn_dispatch_t::response::lock> lock_slot_t;

}}
#endif
