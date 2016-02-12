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

#pragma once

#include "cocaine/api/unicorn.hpp"

#include "cocaine/detail/zookeeper/connection.hpp"

#include <cocaine/idl/unicorn.hpp>

#include <cocaine/unicorn/writable.hpp>

namespace cocaine { namespace unicorn {

//zookeeper::cfg_t make_zk_config(const dynamic_t& args);

/**
* Serializes service representation of value to zookepeers representation.
* Currently ZK store msgpacked data, and service uses cocaine::dynamic_t
*/
zookeeper::value_t
serialize(const value_t& val);

/**
* Unserializes zookepeers representation to service representation.
*/
value_t
unserialize(const zookeeper::value_t& val);

struct zookeeper_scope_t: public api::unicorn_scope_t {
    std::shared_ptr<zookeeper::handler_scope_t> handler_scope;
    virtual
    ~zookeeper_scope_t() {}
};

class zookeeper_t :
    public api::unicorn_t
{
public:

    struct context_t {
        logging::logger_t& log;
        zookeeper::connection_t& zk;
    };

    /**
    * Typedefs for used writable helpers
    */
    typedef api::unicorn_t::writable_ptr writable_ptr;

    zookeeper_t(cocaine::context_t& context, const std::string& name, const dynamic_t& args);

    ~zookeeper_t();

    virtual
    api::unicorn_scope_ptr
    put(writable_ptr::put result,
        const unicorn::path_t& path,
        const unicorn::value_t& value,
        unicorn::version_t version
    );

    virtual
    api::unicorn_scope_ptr
    get(writable_ptr::get result,
        const unicorn::path_t& path
    );

    virtual
    api::unicorn_scope_ptr
    create(writable_ptr::create result,
           const unicorn::path_t& path,
           const unicorn::value_t& value,
           bool ephemeral = false,
           bool sequence = false);

    virtual
    api::unicorn_scope_ptr
    del(writable_ptr::del result,
        const unicorn::path_t& path,
        unicorn::version_t version);

    virtual
    api::unicorn_scope_ptr
    subscribe(writable_ptr::subscribe result,
              const unicorn::path_t& path);

    virtual
    api::unicorn_scope_ptr
    children_subscribe(writable_ptr::children_subscribe result,
                       const unicorn::path_t& path);

    virtual
    api::unicorn_scope_ptr
    increment(writable_ptr::increment result,
              const unicorn::path_t& path,
              const unicorn::value_t& value);

    virtual
    api::unicorn_scope_ptr
    lock(writable_ptr::lock result,
         const unicorn::path_t& path);

private:
    const std::unique_ptr<logging::logger_t> log;
    zookeeper::session_t zk_session;
    zookeeper::connection_t zk;
};

}}
