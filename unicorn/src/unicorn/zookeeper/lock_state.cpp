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

#include "cocaine/detail/unicorn/zookeeper/lock_state.hpp"
#include "cocaine/detail/unicorn/zookeeper/lock.hpp"

#include "cocaine/detail/zookeeper/errors.hpp"

#include <blackhole/logger.hpp>

#include <cocaine/logging.hpp>

namespace cocaine { namespace unicorn {

lock_state_t::lock_state_t(const zookeeper_t::context_t& _ctx) : ctx(_ctx),
                                                                 lock_created(false),
                                                                 lock_released(false),
                                                                 discarded(false),
                                                                 created_path(),
                                                                 access_mutex(),
                                                                 zk_scope()
{}

lock_state_t::~lock_state_t(){
    release();
}

void
lock_state_t::release() {
    bool should_release = false;
    {
        std::lock_guard<std::mutex> guard(access_mutex);
        should_release = lock_created && !lock_released;
        lock_released = lock_released || should_release;
    }
    if(should_release) {
        release_impl();
    }
}

bool
lock_state_t::release_if_discarded() {
    bool should_release = false;
    {
        std::lock_guard<std::mutex> guard(access_mutex);
        if(discarded && lock_created && !lock_released) {
            should_release = true;
            lock_released = true;
        }
    }
    if(should_release) {
        release_impl();
    }
    return lock_released;
}

void
lock_state_t::discard() {
    COCAINE_LOG_ERROR(ctx.log, "lock_action_t::discard");
    bool should_release = false;
    {
        std::lock_guard<std::mutex> guard(access_mutex);
        discarded = true;
        should_release = lock_created && !lock_released;
        lock_released = lock_released || should_release;
    }
    if(should_release) {
        release_impl();
    }
}

bool
lock_state_t::set_lock_created(path_t _created_path) {
    //Safe as there is only one call to this
    created_path = std::move(_created_path);
    COCAINE_LOG_DEBUG(ctx.log, "created lock: {}", created_path);
    //Can not be removed before created.
    assert(!lock_released);
    bool should_release = false;
    {
        std::lock_guard<std::mutex> guard(access_mutex);
        lock_created = true;
        if(discarded) {
            lock_released = true;
            should_release = true;
        }
    }
    if(should_release) {
        release_impl();
    }
    return !lock_released;
}

void
lock_state_t::release_impl() {
    COCAINE_LOG_DEBUG(ctx.log, "release lock: {}", created_path);
    try {
        std::unique_ptr<release_lock_action_t> h(new release_lock_action_t(ctx));
        ctx.zk.del(created_path, not_existing_version, std::move(h));
    } catch(const std::system_error& e) {
        COCAINE_LOG_WARNING(ctx.log, "ZK exception during delete of lock: {}. Reconnecting to discard lock for sure.", e.what());
        try {
            ctx.zk.reconnect();
        } catch(const std::system_error& e2) {
            COCAINE_LOG_WARNING(ctx.log, "give up on deleting lock. Exception: {}", e2.what());
        }
    }
}

}} // namespace cocaine::unicorn
