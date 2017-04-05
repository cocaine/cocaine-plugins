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
#include "cocaine/detail/unicorn/zookeeper.hpp"
#include "cocaine/detail/zookeeper/handler.hpp"

namespace cocaine { namespace unicorn {

class lock_state_t :
    public api::unicorn_scope_t,
    public std::enable_shared_from_this<lock_state_t>
{
public:
    lock_state_t(zookeeper_t::context_t _ctx);

    ~lock_state_t();

    lock_state_t(const lock_state_t& other) = delete;

    lock_state_t& operator=(const lock_state_t& other) = delete;

    void
    release();

    bool
    release_if_discarded();

    virtual void
    close() {
        discard();
    }

    void
    discard();

    bool
    set_lock_created(unicorn::path_t created_path);

    zookeeper::handler_scope_t handler_scope;

private:
    void
    release_impl();

    zookeeper_t::context_t ctx;
    bool lock_created;
    bool lock_released;
    bool discarded;
    unicorn::path_t lock_path;
    unicorn::path_t created_path;
    std::mutex access_mutex;
    zookeeper::handler_scope_t zk_scope;
};

}} //namespace cocaine::unicorn