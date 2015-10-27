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

//zookeeper::cfg_t make_zk_config(const dynamic_t& args);

struct zookeeper_scope_t: public api::unicorn_request_scope_t {
    std::shared_ptr<zookeeper::handler_scope_t> handler_scope;
    virtual
    ~zookeeper_scope_t() {}
};

class zookeeper_t : public api::unicorn_t {
public:

    struct context_t {
        cocaine::logging::log_t& log;
        zookeeper::connection_t& zk;
    };

    /**
    * Typedefs for result type. Actual result types are in include/cocaine/idl/unicorn.hpp
    */
    typedef api::unicorn_t::response response;

    zookeeper_t(cocaine::context_t& context, const std::string& name, const dynamic_t& args);

    virtual
    api::unicorn_scope_ptr
    put(
        writable_helper<response::put_result>::ptr result,
        unicorn::path_t path,
        unicorn::value_t value,
        unicorn::version_t version
    );

    virtual
    api::unicorn_scope_ptr
    get(
        writable_helper<response::get_result>::ptr result,
        unicorn::path_t path
    );

    virtual
    api::unicorn_scope_ptr
    create(
        writable_helper<response::create_result>::ptr result,
        unicorn::path_t path,
        unicorn::value_t value,
        bool ephemeral = false,
        bool sequence = false
    );

    virtual
    api::unicorn_scope_ptr
    del(
        writable_helper<response::del_result>::ptr result,
        unicorn::path_t path,
        unicorn::version_t version
    );

    virtual
    api::unicorn_scope_ptr
    subscribe(
        writable_helper<response::subscribe_result>::ptr result,
        unicorn::path_t path
    );

    virtual
    api::unicorn_scope_ptr
    children_subscribe(
        writable_helper<response::children_subscribe_result>::ptr result,
        unicorn::path_t path
    );

    virtual
    api::unicorn_scope_ptr
    increment(
        writable_helper<response::increment_result>::ptr result,
        unicorn::path_t path,
        unicorn::value_t value
    );

    virtual
    api::unicorn_scope_ptr
    lock(
        writable_helper<response::lock_result>::ptr result,
        unicorn::path_t path
    );

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

private:
    const std::unique_ptr<logging::log_t> log;
    zookeeper::session_t zk_session;
    zookeeper::connection_t zk;
};
}}

#endif
