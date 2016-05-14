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

#include "cocaine/detail/unicorn/zookeeper/create.hpp"

#include "cocaine/detail/future.hpp"
#include "cocaine/detail/zookeeper/errors.hpp"

#include <cocaine/logging.hpp>

#include <blackhole/logger.hpp>


namespace cocaine { namespace unicorn {

create_action_base_t::create_action_base_t(const zookeeper::handler_tag& tag,
                                           const zookeeper_t::context_t& _ctx,
                                           path_t _path,
                                           value_t _value,
                                           bool _ephemeral,
                                           bool _sequence
) : zookeeper::managed_handler_base_t(tag),
    zookeeper::managed_string_handler_base_t(tag),
    depth(0),
    ctx(_ctx),
    path(std::move(_path)),
    initial_value(std::move(_value)),
    encoded_value(serialize(initial_value)),
    ephemeral(_ephemeral),
    sequence(_sequence)
{}

void
create_action_base_t::string_event(int rc, zookeeper::path_t value) {
    try {
        if (rc == ZOK) {
            if (depth == 0) {
                finalize(std::move(value));
            } else if (depth == 1) {
                depth--;
                ctx.zk.create(path, encoded_value, ephemeral, sequence, *this);
            } else {
                depth--;
                ctx.zk.create(zookeeper::path_parent(path, depth), "", false, false, *this);
            }
        } else if (rc == ZNONODE) {
            depth++;
            ctx.zk.create(zookeeper::path_parent(path, depth), "", false, false, *this);
        } else {
            abort(rc);
        }
    } catch(const std::system_error& e) {
        COCAINE_LOG_WARNING(ctx.log, "could not create node hierarchy - {}", e.what());
        abort(e.code().value());
    } catch (const std::exception& e) {
        COCAINE_LOG_WARNING(ctx.log, "could not create node hierarchy - {}", e.what());
        abort(ZSYSTEMERROR);
    }
}


create_action_t::create_action_t(const zookeeper::handler_tag& tag,
                                 const zookeeper_t::context_t& _ctx,
                                 api::unicorn_t::callback::create _callback,
                                 path_t _path,
                                 value_t _value,
                                 bool _ephemeral,
                                 bool _sequence
) : zookeeper::managed_handler_base_t(tag),
    create_action_base_t(tag, _ctx, std::move(_path), std::move(_value), _ephemeral, _sequence),
    callback(std::move(_callback))
{}

void
create_action_t::finalize(zookeeper::value_t) {
    callback(make_ready_future(true));
}

void
create_action_t::abort(int rc) {
    auto ec = make_error_code(error::zookeeper_errors(rc));
    callback(make_exceptional_future<bool>(std::move(ec)));
}

}} // namespace cocaine::unicorn
