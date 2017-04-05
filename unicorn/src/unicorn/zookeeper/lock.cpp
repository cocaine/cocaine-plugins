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

#include "cocaine/detail/zookeeper/errors.hpp"

#include <cocaine/errors.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/utility/future.hpp>

#include <blackhole/logger.hpp>

namespace cocaine { namespace unicorn {

lock_action_t::lock_action_t(const zookeeper::handler_tag& tag,
                             zookeeper_t::context_t _ctx,
                             std::shared_ptr<lock_state_t> _state,
                             path_t _path,
                             path_t _folder,
                             value_t _value,
                             api::unicorn_t::callback::lock _callback
) : zookeeper::managed_handler_base_t(tag),
    create_action_base_t(tag, std::move(_ctx), std::move(_path), std::move(_value), true, true),
    zookeeper::managed_strings_stat_handler_base_t(tag),
    zookeeper::managed_stat_handler_base_t(tag),
    zookeeper::managed_watch_handler_base_t(tag),
    state_lock(std::move(_state)),
    state(state_lock),
    callback(std::move(_callback)),
    folder(std::move(_folder))
{
}

lock_action_t::~lock_action_t() {
    COCAINE_LOG_DEBUG(ctx.log, "lock handler dtor", {{"full_node_name", created_node_name}});
}

void
lock_action_t::children_event(int rc, std::vector<std::string> children, zookeeper::node_stat const& /*stat*/) {
    /**
    * Here we assume:
    *  * nobody has written trash data(any data except locks) in specified directory
    *  * sequence number issued by zookeeper is comaparable by string comparision.
    *
    *  First is responsibility of clients.
    *  Second is true for zookeeper at least 3.4.6,
    */
    if(rc) {
        auto ec = cocaine::error::make_error_code(cocaine::error::zookeeper_errors(rc));
        COCAINE_LOG_ERROR(ctx.log,
                          "could not subscribe for lock({}): {}",
                          ec.value(),
                          ec.message(),
                          blackhole::attribute_list{{"full_node_name", created_node_name}});
        return callback(make_exceptional_future<bool>(std::move(ec)));
    }
    path_t next_min_node = created_node_name;
    for(size_t i = 0; i < children.size(); i++) {
        /**
        * Skip most obvious "trash" data.
        */
        if(!zookeeper::is_valid_sequence_node(children[i])) {
            continue;
        }
        if(children[i] < next_min_node) {
            if(next_min_node == created_node_name) {
                next_min_node.swap(children[i]);
            } else {
                if(children[i] > next_min_node) {
                    next_min_node.swap(children[i]);
                }
            }
        }
    }
    if(next_min_node == created_node_name) {
        COCAINE_LOG_DEBUG(ctx.log, "lock aquired", {{"full_node_name", created_node_name}});
        if(auto s = state.lock()) {
            // TODO: Do we really need this check?
            if (!s->release_if_discarded()) {
                callback(make_ready_future(true));
            } else {
                COCAINE_LOG_DEBUG(ctx.log,
                                  "lock watch event (lock was already released)",
                                  blackhole::attribute_list{{"full_node_name", created_node_name}});
            }
        } else {
            COCAINE_LOG_ERROR(ctx.log,
                              "lock aquired, but state is missing",
                              blackhole::attribute_list{{"full_node_name", created_node_name}});
        }
        return;
    } else {
        try {
            ctx.zk.exists(folder + "/" + next_min_node, *this, *this);
        } catch(const std::system_error& e) {
            COCAINE_LOG_WARNING(ctx.log,
                                "exception during exists command - {}",
                                error::to_string(e),
                                blackhole::attribute_list{{"full_node_name", created_node_name}});
            callback(make_exceptional_future<bool>(e));
            if(auto s = state.lock()) {
                s->release();
            }
        }
    }
}

void
lock_action_t::stat_event(int rc, zookeeper::node_stat const& /*stat*/) {
    /* If next lock in queue disappeared during request - issue childs immediately. */
    if(rc == ZNONODE) {
        try {
            ctx.zk.childs(folder, *this);
        } catch(const std::system_error& e) {
            COCAINE_LOG_WARNING(ctx.log,
                                "exception during childs command - {}",
                                error::to_string(e),
                                blackhole::attribute_list{{"full_node_name", created_node_name}});
            callback(make_exceptional_future<bool>(e));
            if(auto s = state.lock()) {
                s->release();
            }
        }
    }
}

void
lock_action_t::watch_event(int /*type*/, int /*zk_state*/, path_t /*path*/) {
    try {
        ctx.zk.childs(folder, *this);
    } catch(const std::system_error& e) {
        COCAINE_LOG_WARNING(ctx.log,
                            "watch event, exception during childs command - {}",
                            error::to_string(e),
                            blackhole::attribute_list{{"full_node_name", created_node_name}});
        callback(make_exceptional_future<bool>(e));
        if(auto s = state.lock()) {
            s->release();
        }
    }
}

void
lock_action_t::finalize(zookeeper::path_t created_path) {
    created_node_name = zookeeper::get_node_name(created_path);

    if(state_lock->set_lock_created(std::move(created_path))) {
        COCAINE_LOG_DEBUG(ctx.log, "lock aquired", blackhole::attribute_list{{"full_node_name", created_node_name}});
        try {
            ctx.zk.childs(folder, *this);
        } catch(const std::system_error& e) {
            COCAINE_LOG_WARNING(ctx.log,
                                "error in childs request after lock aquisition - {}",
                                error::to_string(e),
                                blackhole::attribute_list{{"full_node_name", created_node_name}});
            callback(make_exceptional_future<bool>(e));
            state_lock->release();
        }
    }
    state_lock.reset();
}

void
lock_action_t::abort(int rc) {
    auto ec = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
    COCAINE_LOG_WARNING(ctx.log,
                        "lock creation aborted - {}",
                        ec.message(),
                        blackhole::attribute_list{{"full_node_name", created_node_name}});
    callback(make_exceptional_future<bool>(std::move(ec)));
}

release_lock_action_t::release_lock_action_t(zookeeper_t::context_t _ctx) :
    ctx(_ctx)
{
    COCAINE_LOG_DEBUG(ctx.log, "release lock action created");
}

void
release_lock_action_t::void_event(int rc) {
    COCAINE_LOG_DEBUG(ctx.log, "release lock action done - {}", rc);
    if(rc && rc != ZCLOSING) {
        auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
        COCAINE_LOG_WARNING(ctx.log, "ZK error during lock delete: {}. Reconnecting to discard lock for sure", code.message().c_str());
        try {
            ctx.zk.reconnect();
        } catch(const std::system_error& e) {
            COCAINE_LOG_ERROR(ctx.log, "giving up: {}", error::to_string(e));
        }
    }
}

}} // namespace cocaine::unicorn
