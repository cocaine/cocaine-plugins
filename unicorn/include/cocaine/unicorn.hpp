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

/*
Можно создать внутри диспетча хранилище в котором хранить все созданные вотчеры и хендлеры.
в  зукипер передавать УКАЗАТЕЛЬ на weak_ptr. когда коллбек дергается - мы пытаемся кастануть к shared_ptr и если не вышло,
ничего не делаем.  weak_ptr в любом случае удаляем. Так будет чуть больше аллокаций, но можно иметь один класс для обработки и
не нужен менеджер.
От dynamic_cast можно избавиться если сделать connection темплейтным, но это может грозить неправильным использованием.
 */

using namespace cocaine::unicorn;
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
    friend class lock_slot_t;
    friend void release_lock(unicorn_service_t* service, const path_t& path);
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
        typedef deferred<result_of<io::unicorn::acquire>::type> acquire;
    };

    unicorn_dispatch_t(const std::string& name, unicorn_service_t* parent);

    response::put
    put(path_t path, value_t value, version_t version);

    response::create
    create(path_t path, value_t value);

    response::del
    del(path_t path, version_t version);

    response::subscribe
    subscribe(path_t path);

    response::lsubscribe
    lsubscribe(path_t path);

    response::increment
    increment(path_t path, value_t value);


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

    friend class lock_slot_t;
    friend class distributed_lock_t;
private:
    scope_ptr handler_scope;
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

class lock_slot_t :
    public io::basic_slot<io::unicorn::lock>
{
public:
    lock_slot_t(unicorn_service_t* service);

    typedef io::basic_slot<io::unicorn::lock>::dispatch_type dispatch_type;
    typedef io::basic_slot<io::unicorn::lock>::tuple_type tuple_type;
    typedef io::basic_slot<io::unicorn::lock>::upstream_type upstream_type;

    virtual
    boost::optional<std::shared_ptr<const dispatch_type>>
        operator()(tuple_type&& args, upstream_type&& upstream);

private:
    unicorn_service_t* service;
};

class distributed_lock_t:
    public dispatch<io::unicorn_locked_tag>,
    public std::enable_shared_from_this<distributed_lock_t>
{
public:
    distributed_lock_t(const std::string& name, path_t _path, unicorn_service_t* _service);

    virtual
    void
    discard(const std::error_code& ec) const;

    unicorn_dispatch_t::response::acquire
    acquire();

    struct put_ephemeral_context_t;
private:
    struct state_t {
        bool lock_acquired;
        bool discarded;
        state_t() :
            lock_acquired(false),
            discarded(false)
        {}
    };
    mutable synchronized<state_t> state;
    zookeeper::handler_scope_t handler_scope;
    path_t path;
    unicorn_service_t* service;
};

}}
#endif
