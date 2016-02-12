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

#include "cocaine/detail/unicorn/zookeeper/increment.hpp"

#include "cocaine/detail/zookeeper/errors.hpp"

#include <blackhole/logger.hpp>

#include <cocaine/logging.hpp>

namespace cocaine { namespace unicorn {

increment_action_t::increment_action_t(const zookeeper::handler_tag& tag,
                                       const zookeeper_t::context_t& _ctx,
                                       api::unicorn_t::writable_ptr::increment _result,
                                       path_t _path,
                                       value_t _increment
) : managed_handler_base_t(tag),
    create_action_base_t(tag, _ctx, std::move(_path), std::move(_increment), false, false),
    managed_stat_handler_base_t(tag),
    managed_data_handler_base_t(tag),
    ctx(_ctx),
    result(std::move(_result)),
    total()
{}


void
increment_action_t::stat_event(int rc, zookeeper::node_stat const& stat) {
    if (rc == ZOK) {
        result->write(versioned_value_t(total, stat.version));
    } else if (rc == ZBADVERSION) {
        try {
            ctx.zk.get(path, *this);
        } catch(const std::system_error& e) {
            result->abort(e.code());
            COCAINE_LOG_WARNING(ctx.log, "failure during increment get: {}", e.what());
        }
    }
}

void
increment_action_t::data_event(int rc, zookeeper::value_t value, const zookeeper::node_stat& stat) {
    try {
        if(rc == ZNONODE) {
            ctx.zk.create(path, encoded_value, ephemeral, sequence, *this);
        } else if (rc != ZOK) {
            auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
            result->abort(code, "increment error during value get");
        } else {
            value_t parsed;
            if (!value.empty()) {
                parsed = unserialize(value);
            }
            if (stat.numChildren != 0) {
                result->abort(cocaine::error::child_not_allowed, "increment error during value get");
                return;
            }
            if (!parsed.is_double() && !parsed.is_int() && !parsed.is_uint()) {
                result->abort(cocaine::error::invalid_type, "increment error during value get");
                return;
            }
            assert(initial_value.is_uint() || initial_value.is_uint() || initial_value.is_double());
            if (parsed.is_double() || initial_value.is_double()) {
                total = parsed.to<double>() + initial_value.to<double>();
                ctx.zk.put(path, serialize(total), stat.version, *this);
            } else {
                total = parsed.to<int64_t>() + initial_value.to<int64_t>();
                ctx.zk.put(path, serialize(total), stat.version, *this);
            }
        }
    } catch(const std::system_error& e) {
        COCAINE_LOG_WARNING(ctx.log, "failure during get action of increment: {}", e.what());
        result->abort(e.code());
    }
}

void
increment_action_t::finalize(zookeeper::value_t) {
    result->write(versioned_value_t(initial_value, version_t()));
}

void
increment_action_t::abort(int rc) {
    auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
    result->abort(code);
}

}} // namespace cocaine::unicorn
