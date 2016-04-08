/*
    Copyright (c) 2015+ Anton Matveenko <antmat@yandex-team.ru>
    Copyright (c) 2015+ Other contributors as noted in the AUTHORS file.
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

#pragma once

#include "zookeeper/zookeeper.h"

#include <system_error>

namespace cocaine { namespace error {

enum zookeeper_errors {
    invalid_connection_endpoint = 1,
    could_not_connect
};

auto
zookeeper_category() -> const std::error_category&;

constexpr size_t zookeeper_category_id = 0x40FF;

auto
make_error_code(zookeeper_errors code) -> std::error_code;

}}

namespace std {

template<>
struct is_error_code_enum<cocaine::error::zookeeper_errors>:
public true_type
{ };

}
