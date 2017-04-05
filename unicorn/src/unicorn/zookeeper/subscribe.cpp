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

#include "cocaine/detail/unicorn/zookeeper/subscribe.hpp"

#include "cocaine/detail/zookeeper/errors.hpp"

#include <cocaine/errors.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/utility/future.hpp>

#include <blackhole/logger.hpp>


namespace cocaine { namespace unicorn {

subscribe_action_t::subscribe_action_t(const zookeeper::handler_tag& tag,
                                       api::unicorn_t::callback::subscribe _callback,
                                       zookeeper_t::context_t _ctx,
                                       path_t _path
) : managed_handler_base_t(tag),
    managed_data_handler_base_t(tag),
    managed_watch_handler_base_t(tag),
    managed_stat_handler_base_t(tag),
    callback(std::move(_callback)),
    ctx(std::move(_ctx)),
    write_lock(),
    last_version(unicorn::min_version),
    path(std::move(_path))
{}

void
subscribe_action_t::data_event(int rc, std::string value, const zookeeper::node_stat& stat) {
    if(rc == ZNONODE) {
        if(last_version != min_version && last_version != not_existing_version) {
            auto ec = cocaine::error::make_error_code(cocaine::error::zookeeper_errors(rc));
            callback(make_exceptional_future<versioned_value_t>(ec));
        } else {
            // Write that node is not exist to client only first time.
            // After that set a watch to see when it will appear
            if(last_version == min_version) {
                std::lock_guard<std::mutex> guard(write_lock);
                if (not_existing_version > last_version) {
                    callback(make_ready_future(versioned_value_t(value_t(), not_existing_version)));
                }
            }
            try {
                ctx.zk.exists(path, *this, *this);
            } catch(const std::system_error& e) {
                COCAINE_LOG_WARNING(ctx.log, "failure during subscription(get): {}", e.what());
                callback(make_exceptional_future<versioned_value_t>(e));
            }
        }
    } else if (rc != 0) {
        auto ec = cocaine::error::make_error_code(cocaine::error::zookeeper_errors(rc));
        callback(make_exceptional_future<versioned_value_t>(ec));
    } else if (stat.numChildren != 0) {
        auto ec = make_error_code(cocaine::error::child_not_allowed);
        callback(make_exceptional_future<versioned_value_t>(ec));
    } else {
        version_t new_version(stat.version);
        std::lock_guard<std::mutex> guard(write_lock);
        if (new_version > last_version) {
            last_version = new_version;
            value_t val;
            try {
                callback(make_ready_future(versioned_value_t(unserialize(value), new_version)));
            } catch(const std::system_error& e) {
                callback(make_exceptional_future<versioned_value_t>(e));
            }
        }
    }
}

void
subscribe_action_t::stat_event(int rc, zookeeper::node_stat const&) {
    // Someone created a node in a gap between
    // we received nonode and issued exists
    if(rc == ZOK) {
        try {
            ctx.zk.get(path, *this, *this);
        } catch(const std::system_error& e)  {
            COCAINE_LOG_WARNING(ctx.log, "failure during subscription(stat): {}", e.what());
            callback(make_exceptional_future<versioned_value_t>(e));
        }
    }
}

void
subscribe_action_t::watch_event(int /* type */, int /* state */, zookeeper::path_t) {
    try {
        ctx.zk.get(path, *this, *this);
    } catch(const std::system_error& e)  {
        COCAINE_LOG_WARNING(ctx.log, "failure during subscription(watch): {}", e.what());
        callback(make_exceptional_future<versioned_value_t>(e));
    }
}

}} // namespace cocaine::unicorn
