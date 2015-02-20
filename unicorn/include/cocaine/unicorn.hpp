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

using namespace cocaine::unicorn;
namespace cocaine { namespace service {

/**
* Service for providing access to unified configuration service.
* Currently we use zookeeper as backend.
* For protocol methods description see include/cocaine/idl/unicorn.hpp
*/
class unicorn_t:
    public api::service_t,
    public dispatch<io::unicorn_tag>
{
public:

    /**
    * Typedefs for result type. Actual result types are in include/cocaine/idl/unicorn.hpp
    */
    struct response {
        typedef deferred<result_of<io::unicorn::put>::type> put;
        typedef deferred<result_of<io::unicorn::del>::type> del;
        typedef deferred<result_of<io::unicorn::increment>::type> increment;
        typedef streamed<result_of<io::unicorn::subscribe>::type> subscribe;
        typedef streamed<result_of<io::unicorn::lsubscribe>::type> lsubscribe;
        typedef deferred<result_of<io::unicorn::acquire>::type> acquire;
    };

    unicorn_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args);

    response::put
    put(path_t path, value_t value, version_t version);

    response::del
    del(path_t path, version_t version);

    response::subscribe
    subscribe(path_t path, version_t upstream_version);

    response::lsubscribe
    lsubscribe(path_t path, version_t upstream_version);

    response::increment
    increment(path_t path, value_t value);

    virtual
    const io::basic_dispatch_t&
    prototype() const {
        return *this;
    }

    /**
    * Callbacks to handle async ZK responses
    */
    struct nonode_action_t;

    struct subscribe_context_base_t;

    struct subscribe_context_t;
    struct subscribe_action_t;
    struct subscribe_nonode_action_t;
    struct subscribe_watch_handler_t;

    struct lsubscribe_context_t;
    struct lsubscribe_action_t;
    struct lsubscribe_watch_handler_t;
    
    struct put_context_base_t;
    struct put_context_t;
    struct put_action_t;
    struct put_badversion_action_t;

    struct del_action_t;

    struct increment_context_t;
    struct increment_action_t;
    struct increment_get_action_t;
    struct increment_set_action_t;

    friend class lock_slot_t;
    friend class distributed_lock_t;
private:
    zookeeper::session_t zk_session;
    zookeeper::connection_t zk;
    std::shared_ptr<cocaine::logging::log_t> log;
};



class lock_slot_t :
    public io::basic_slot<io::unicorn::lock>
{
public:
    lock_slot_t(unicorn_t *const parent_);

    typedef io::basic_slot<io::unicorn::lock>::dispatch_type dispatch_type;
    typedef io::basic_slot<io::unicorn::lock>::tuple_type tuple_type;
    typedef io::basic_slot<io::unicorn::lock>::upstream_type upstream_type;

    virtual
    boost::optional<std::shared_ptr<const dispatch_type>>
    operator()(tuple_type&& args, upstream_type&& upstream);

private:
    unicorn_t *const parent;
};



class distributed_lock_t:
    public dispatch<io::unicorn_locked_tag>
{
public:
    distributed_lock_t(const std::string& name, path_t _path, unicorn_t* _parent);

    virtual
    void
    discard(const std::error_code& ec) const;

    unicorn_t::response::acquire
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
    path_t path;
    unicorn_t* parent;
};

}}
#endif
