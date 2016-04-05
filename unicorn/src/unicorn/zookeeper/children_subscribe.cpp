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

#include "cocaine/detail/unicorn/zookeeper/children_subscribe.hpp"

#include "cocaine/detail/future.hpp"
#include "cocaine/detail/zookeeper/errors.hpp"

#include <cocaine/errors.hpp>
#include <cocaine/logging.hpp>

#include <blackhole/logger.hpp>

namespace cocaine { namespace unicorn {

children_subscribe_action_t::children_subscribe_action_t(const zookeeper::handler_tag& tag,
                                                         api::unicorn_t::callback::children_subscribe _callback,
                                                         const zookeeper_t::context_t& _ctx,
                                                         path_t _path
) : managed_handler_base_t(tag),
    managed_strings_stat_handler_base_t(tag),
    managed_watch_handler_base_t(tag),
    callback(std::move(_callback)),
    ctx(_ctx),
    write_lock(),
    last_version(unicorn::min_version),
    path(std::move(_path))
{ }


void
children_subscribe_action_t::children_event(int rc, std::vector<std::string> childs, const zookeeper::node_stat& stat) {
    if (rc != 0) {
        auto ec = make_error_code(cocaine::error::zookeeper_errors(rc));
        auto future = make_exceptional_future<result_t>(ec);
        callback(std::move(future));
    } else {
        version_t new_version(stat.cversion);
        std::lock_guard <std::mutex> guard(write_lock);
        if (new_version > last_version) {
            last_version = new_version;
            callback(make_ready_future(std::make_tuple(new_version, std::move(childs))));
        }
    }
}

void
children_subscribe_action_t::watch_event(int /*type*/, int /*state*/, zookeeper::path_t /*path*/) {
    try {
        ctx.zk.childs(path, *this, *this);
    } catch (const std::system_error& e) {
        COCAINE_LOG_WARNING(ctx.log, "failure during subscription for childs: {}", error::to_string(e));
        callback(make_exceptional_future<result_t, std::system_error>(e));
    }
}
}} // namespace cocaine::unicorn
