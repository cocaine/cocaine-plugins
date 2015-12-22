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

#include "cocaine/detail/unicorn/zookeeper/lock.hpp"

#include "cocaine/unicorn/errors.hpp"

namespace cocaine { namespace unicorn {

lock_action_t::lock_action_t(const zookeeper::handler_tag& tag,
                             const zookeeper_t::context_t& _ctx,
                             std::shared_ptr<lock_state_t> _state,
                             path_t _path,
                             path_t _folder,
                             value_t _value,
                             api::unicorn_t::writable_ptr::lock _result
) : zookeeper::managed_handler_base_t(tag),
    create_action_base_t(tag, _ctx, std::move(_path), std::move(_value), true, true),
    zookeeper::managed_strings_stat_handler_base_t(tag),
    zookeeper::managed_stat_handler_base_t(tag),
    zookeeper::managed_watch_handler_base_t(tag),
    state(std::move(_state)),
    result(std::move(_result)),
    folder(std::move(_folder))
{}

void
lock_action_t::operator()(int rc, std::vector<std::string> childs, zookeeper::node_stat const& /*stat*/) {
    /**
    * Here we assume:
    *  * nobody has written trash data(any data except locks) in specified directory
    *  * sequence number issued by zookeeper is comaparable by string comparision.
    *
    *  First is responsibility of clients.
    *  Second is true for zookeeper at least 3.4.6,
    */
    if(rc) {
        auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
        COCAINE_LOG_ERROR(ctx.log, "Could not subscribe for lock(%i). : %s", code.value(), code.message().c_str());
        result->abort(code);
        return;
    }
    path_t next_min_node = created_node_name;
    for(size_t i = 0; i < childs.size(); i++) {
        /**
        * Skip most obvious "trash" data.
        */
        if(!zookeeper::is_valid_sequence_node(childs[i])) {
            continue;
        }
        if(childs[i] < next_min_node) {
            if(next_min_node == created_node_name) {
                next_min_node.swap(childs[i]);
            } else {
                if(childs[i] > next_min_node) {
                    next_min_node.swap(childs[i]);
                }
            }
        }
    }
    if(next_min_node == created_node_name) {
        if(!state->release_if_discarded()) {
            result->write(true);
        }
        return;
    } else {
        try {
            ctx.zk.exists(folder + "/" + next_min_node, *this, *this);
        } catch(const std::system_error& e) {
            result->abort(e.code());
            state->release();
        }
    }
}

void
lock_action_t::operator()(int rc, zookeeper::node_stat const& /*stat*/) {
    /* If next lock in queue disappeared during request - issue childs immediately. */
    if(rc == ZNONODE) {
        try {
            ctx.zk.childs(folder, *this);
        } catch(const std::system_error& e) {
            result->abort(e.code());
            state->release();
        }
    }
}

void
lock_action_t::operator()(int /*type*/, int /*zk_state*/, path_t /*path*/) {
    try {
        ctx.zk.childs(folder, *this);
    } catch(const std::system_error& e) {
        result->abort(e.code());
        state->release();
    }
}

void
lock_action_t::finalize(zookeeper::value_t value) {
    zookeeper::path_t created_path = std::move(value);
    created_node_name = zookeeper::get_node_name(created_path);
    if(state->set_lock_created(std::move(created_path))) {
        try {
            ctx.zk.childs(folder, *this);
        } catch(const std::system_error& e) {
            result->abort(e.code());
            state->release();
        }
    }
}

void
lock_action_t::abort(int rc) {
    auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
    result->abort(code);
}

release_lock_action_t::release_lock_action_t(const zookeeper_t::context_t& _ctx) :
ctx(_ctx)
{}

void
release_lock_action_t::operator()(int rc) {
    if(rc) {
        auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
        COCAINE_LOG_WARNING(ctx.log, "ZK error during lock delete: %s. Reconnecting to discard lock for sure.", code.message().c_str());
        ctx.zk.reconnect();
    }
}

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

void lock_state_t::schedule_for_lock(/*scope_ptr _zk_scope*/) {
    //TODO Take a look if this is correct
    //zk_scope = std::move(_zk_scope);
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
    COCAINE_LOG_DEBUG(ctx.log, "created lock: %s", created_path.c_str());
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
    //zk_scope.reset();
    return !lock_released;
}

void
lock_state_t::abort_lock_creation() {
    //zk_scope.reset();
}

void
lock_state_t::release_impl() {
    COCAINE_LOG_DEBUG(ctx.log, "release lock: %s", created_path.c_str());
    try {
        std::unique_ptr<release_lock_action_t> h(new release_lock_action_t(ctx));
        ctx.zk.del(created_path, NOT_EXISTING_VERSION, std::move(h));
    } catch(const std::system_error& e) {
        COCAINE_LOG_WARNING(ctx.log, "ZK exception during delete of lock: %s. Reconnecting to discard lock for sure.", e.what());
        try {
            ctx.zk.reconnect();
        } catch(const std::system_error& e2) {
            COCAINE_LOG_WARNING(ctx.log, "give up on deleting lock. Exception: %s", e2.what());
        }
    }
}

}} // namespace cocaine::unicorn