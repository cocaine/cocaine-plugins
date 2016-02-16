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

namespace cocaine { namespace unicorn {

del_action_t::del_action_t(api::unicorn_t::writable_ptr::del _result) : result(std::move(_result)) {
    // pass
}

void
del_action_t::void_event(int rc) {
    if (rc) {
        auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
        result->abort(code);
    } else {
        result->write(true);
    }
}

}} //namespace cocaine::unicorn
