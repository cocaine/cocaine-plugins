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

#ifndef COCAINE_UNICORN_ZOOKEEPER_API_HPP
#define COCAINE_UNICORN_ZOOKEEPER_API_HPP

#include <cocaine/idl/unicorn.hpp>
#include <cocaine/zookeeper/connection.hpp>
#include <cocaine/unicorn/writable.hpp>
#include "cocaine/unicorn/api.hpp"

namespace cocaine { namespace unicorn {

zookeeper::cfg_t make_zk_config(const dynamic_t& args);

class zookeeper_api_t : public api_t {
public:

    struct context_t {
        cocaine::logging::log_t& log;
        zookeeper::connection_t& zk;
    };

    typedef std::shared_ptr <zookeeper::handler_scope_t> scope_ptr;

    /**
    * Typedefs for result type. Actual result types are in include/cocaine/idl/unicorn.hpp
    */
    typedef api_t::response response;

    zookeeper_api_t(cocaine::logging::log_t& log, zookeeper::connection_t&);

    virtual void
    put(
        writable_helper<response::put_result>::ptr result,
        unicorn::path_t path,
        unicorn::value_t value,
        unicorn::version_t version
    );

    virtual void
    get(
        writable_helper<response::get_result>::ptr result,
        unicorn::path_t path
    );

    virtual void
    create(
        writable_helper<response::create_result>::ptr result,
        unicorn::path_t path,
        unicorn::value_t value,
        bool ephemeral = false,
        bool sequence = false
    );

    virtual void
    del(
        writable_helper<response::del_result>::ptr result,
        unicorn::path_t path,
        unicorn::version_t version
    );

    virtual void
    subscribe(
        writable_helper<response::subscribe_result>::ptr result,
        unicorn::path_t path
    );

    virtual void
    children_subscribe(
        writable_helper<response::children_subscribe_result>::ptr result,
        unicorn::path_t path
    );

    virtual void
    increment(
        writable_helper<response::increment_result>::ptr result,
        unicorn::path_t path,
        unicorn::value_t value
    );

    virtual void
    lock(
        writable_helper<response::lock_result>::ptr result,
        unicorn::path_t path
    );

    virtual void
    close();

    /**
    * Callbacks to handle async ZK responses
    */
    struct nonode_action_t;

    struct subscribe_action_t;

    struct children_subscribe_action_t;

    struct put_action_t;

    struct create_action_base_t;
    struct create_action_t;

    struct del_action_t;

    struct increment_action_t;
    struct increment_create_action_t;

    struct lock_action_t;

    struct release_lock_action_t;

    class lock_state_t :
        public std::enable_shared_from_this<lock_state_t>
    {
    public:
        lock_state_t(const zookeeper_api_t::context_t& _ctx);
        ~lock_state_t();
        lock_state_t(const lock_state_t& other) = delete;
        lock_state_t& operator=(const lock_state_t& other) = delete;

        void
        schedule_for_lock(scope_ptr _zk_scope);

        void
        release();

        bool
        release_if_discarded();

        void
        discard();

        bool
        set_lock_created(unicorn::path_t created_path);

        void
        abort_lock_creation();
    private:
        void
        release_impl();

        zookeeper_api_t::context_t ctx;
        bool lock_created;
        bool lock_released;
        bool discarded;
        unicorn::path_t lock_path;
        unicorn::path_t created_path;
        std::mutex access_mutex;
        scope_ptr zk_scope;
    };

private:
    std::shared_ptr<lock_state_t> lock_state;
    scope_ptr handler_scope;
    context_t ctx;
};
}}

#endif
