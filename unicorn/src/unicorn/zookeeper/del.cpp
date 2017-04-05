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

#include "cocaine/detail/unicorn/zookeeper/del.hpp"

#include "cocaine/detail/zookeeper/errors.hpp"

#include <cocaine/utility/future.hpp>

namespace cocaine { namespace unicorn {

del_action_t::del_action_t(api::unicorn_t::callback::del _callback) : callback(std::move(_callback)) {
    // pass
}

void
del_action_t::void_event(int rc) {
    if (rc) {
        auto ec = make_error_code(cocaine::error::zookeeper_errors(rc));
        callback(make_exceptional_future<bool>(ec));
    } else {
        callback(make_ready_future(true));
    }
}

}} //namespace cocaine::unicorn
