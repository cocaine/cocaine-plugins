/*
    Copyright (c) 2011-2015 Anton Matveenko <antmat@yandex-team.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COCAINE_UNICOR_ERRORS_HPP
#define COCAINE_UNICOR_ERRORS_HPP

#include "zookeeper/zookeeper.h"

#include <system_error>

namespace cocaine { namespace error {

typedef ZOO_ERRORS zookeeper_errors;

/**
* Unicorn errors related to artificial(non-zookeeper) restrictions of unicorn service.
*/
enum unicorn_errors {
    CHILD_NOT_ALLOWED = 1,
    INVALID_TYPE,
    INVALID_VALUE,
    COULD_NOT_CONNECT,
    UNKNOWN_ERROR,
    HANDLER_SCOPE_RELEASED,
    INVALID_NODE_NAME,
    INVALID_PATH,
    VERSION_NOT_ALLOWED,
    INVALID_CONNECTION_ENDPOINT
};

auto
make_error_code(zookeeper_errors code) -> std::error_code;

auto
make_error_code(unicorn_errors code) -> std::error_code;

}}

namespace std {

template<>
struct is_error_code_enum<cocaine::error::zookeeper_errors>:
    public true_type
{ };

template<>
struct is_error_code_enum<cocaine::error::unicorn_errors>:
    public true_type
{ };

}
#endif // COCAINE_UNICOR_ERRORS_HPP

