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
#include "cocaine/unicorn/path.hpp"
#include "cocaine/unicorn/value.hpp"

#include "cocaine/zookeeper/connection.hpp"
#include "cocaine/zookeeper/session.hpp"

#include <cocaine/api/service.hpp>
#include <cocaine/rpc/dispatch.hpp>

#include <asio/ip/tcp.hpp>
#include <asio/deadline_timer.hpp>

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
    friend void release_lock(unicorn_service_t* service, const unicorn::path_t& path);
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

    typedef std::shared_ptr<zookeeper::handler_scope_t> scope_ptr;
    /**
    * Typedefs for result type. Actual result types are in include/cocaine/idl/unicorn.hpp
    */
    struct response {
        typedef deferred<result_of<io::unicorn::put>::type> put;
        typedef deferred<result_of<io::unicorn::create>::type> create;
        typedef deferred<result_of<io::unicorn::del>::type> del;
        typedef deferred<result_of<io::unicorn::increment>::type> increment;
        typedef streamed<result_of<io::unicorn::subscribe>::type> subscribe;
        typedef streamed<result_of<io::unicorn::lsubscribe>::type> lsubscribe;
        typedef deferred<result_of<io::unicorn::lock>::type> lock;
    };

    unicorn_dispatch_t(const std::string& name, unicorn_service_t* parent);

    response::put
    put(unicorn::path_t path, unicorn::value_t value, unicorn::version_t version);

    response::create
    create(unicorn::path_t path, unicorn::value_t value);

    response::del
    del(unicorn::path_t path, unicorn::version_t version);

    response::subscribe
    subscribe(unicorn::path_t path);

    response::lsubscribe
    lsubscribe(unicorn::path_t path);

    response::increment
    increment(unicorn::path_t path, unicorn::value_t value);

    /**
    * Callbacks to handle async ZK responses
    */
    struct nonode_action_t;

    struct subscribe_action_t;

    struct lsubscribe_action_t;

    struct put_action_t;

    struct create_action_base_t;
    struct create_action_t;

    struct del_action_t;

    struct increment_action_t;
    struct increment_create_action_t;

private:
    scope_ptr handler_scope;
    unicorn_service_t* service;
};

class distributed_lock_t:
    public dispatch<io::unicorn_locked_tag>,
    public std::enable_shared_from_this<distributed_lock_t>
{
public:
    distributed_lock_t(const std::string& name, unicorn_service_t* _service);

    virtual
    void
    discard(const std::error_code& ec) const;

    unicorn_dispatch_t::response::lock
    lock(unicorn::path_t path);

    struct lock_action_t;

    class lock_state_t :
        public std::enable_shared_from_this<lock_state_t>
    {
    public:
        lock_state_t(unicorn_service_t* service);
        ~lock_state_t();
        lock_state_t(const lock_state_t& other) = delete;
        lock_state_t& operator=(const lock_state_t& other) = delete;

        void
        release();

        bool
        release_if_discarded();

        void
        discard();

        bool
        set_lock_created(unicorn::path_t created_path);
    private:
        void
        release_impl();

        unicorn_service_t* service;
        bool lock_created;
        bool lock_released;
        bool discarded;
        unicorn::path_t created_path;
        std::mutex access_mutex;
    };

private:
    mutable std::shared_ptr<lock_state_t> state;
    zookeeper::handler_scope_t handler_scope;
    unicorn::path_t path;
    unicorn_service_t* service;
};

template<class Event, class Method, class Dispatch>
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
        std::shared_ptr<Dispatch> dispatch = std::make_shared<Dispatch>(service->get_name(), service);
        auto callback = std::tuple_cat(std::make_tuple(dispatch.get()), std::move(args));
        auto result = tuple::invoke(std::mem_fn(method), std::move(callback));
        result.attach(std::move(upstream));
        return boost::make_optional<std::shared_ptr<const dispatch_type>>(dispatch);
    }

private:
    unicorn_service_t* service;
    Method method;
};

typedef unicorn_slot_t<io::unicorn::lsubscribe, decltype(&unicorn_dispatch_t::lsubscribe), unicorn_dispatch_t> lsubscribe_slot_t;
typedef unicorn_slot_t<io::unicorn::subscribe,  decltype(&unicorn_dispatch_t::subscribe),  unicorn_dispatch_t> subscribe_slot_t;
typedef unicorn_slot_t<io::unicorn::put,        decltype(&unicorn_dispatch_t::put),        unicorn_dispatch_t> put_slot_t;
typedef unicorn_slot_t<io::unicorn::create,     decltype(&unicorn_dispatch_t::create),     unicorn_dispatch_t> create_slot_t;
typedef unicorn_slot_t<io::unicorn::del,        decltype(&unicorn_dispatch_t::del),        unicorn_dispatch_t> del_slot_t;
typedef unicorn_slot_t<io::unicorn::increment,  decltype(&unicorn_dispatch_t::increment),  unicorn_dispatch_t> increment_slot_t;
typedef unicorn_slot_t<io::unicorn::lock,       decltype(&distributed_lock_t::lock),       distributed_lock_t> lock_slot_t;

}}
#endif
