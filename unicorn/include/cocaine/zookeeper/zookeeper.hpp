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

#ifndef ZOOKEEPER_HPP
#define ZOOKEEPER_HPP

#include <zookeeper/zookeeper.h>

#include <string>

namespace zookeeper {
enum ZOO_EXTRA_ERROR {
    CHILD_NOT_ALLOWED = -1000,
    INVALID_TYPE = -1001,
};
typedef std::string path_t;
typedef std::string value_t;
typedef long version_t;
path_t path_parent(const path_t& path, unsigned int depth);
}

#endif