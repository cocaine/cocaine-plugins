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

#include "cocaine/detail/unicorn/zookeeper/put.hpp"

#include "cocaine/detail/zookeeper/errors.hpp"

#include <blackhole/logger.hpp>

#include <cocaine/logging.hpp>

namespace cocaine { namespace unicorn {

put_action_t::put_action_t( const zookeeper::handler_tag& tag,
                            const zookeeper_t::context_t& _ctx,
                            api::unicorn_t::writable_ptr::put _result,
                            path_t _path,
                            value_t _value,
                            version_t _version
) : managed_handler_base_t(tag),
    managed_stat_handler_base_t(tag),
    managed_data_handler_base_t(tag),
    ctx(_ctx),
    result(std::move(_result)),
    path(std::move(_path)),
    initial_value(std::move(_value)),
    encoded_value(serialize(initial_value)),
    version(std::move(_version))
{}

void
put_action_t::stat_event(int rc, zookeeper::node_stat const& stat) {
    try {
        if (rc == ZBADVERSION) {
            ctx.zk.get(path, *this);
        } else if (rc != 0) {
            auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
            result->abort(code);
        } else {
            result->write(std::make_tuple(true, versioned_value_t(initial_value, stat.version)));
        }
    } catch (const std::system_error& e) {
        COCAINE_LOG_WARNING(ctx.log, "failure during put action: [{}] {} - {}", e.code().value(), e.code().message(), e.what());
    }
}

void
put_action_t::data_event(int rc, zookeeper::value_t value, zookeeper::node_stat const& stat) {
    if (rc) {
        auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
        result->abort(code, "failed get after version mismatch");
    } else {
        try {
            result->write(std::make_tuple(false,versioned_value_t(unserialize(value), stat.version)));
        } catch (const std::system_error& e) {
            result->abort(e.code(), "error during new value get");
        }
    }
}

}} // namespace cocaine::unicorn
