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

/**
* Extensions of zookeeper error used by cocaine.
*/
enum ZOO_EXTRA_ERROR {
    CHILD_NOT_ALLOWED = -1000,
    INVALID_TYPE = -1001,
    INVALID_VALUE = -1002
};

typedef std::string path_t;
typedef std::string value_t;
typedef long version_t;

/**
* Get zookeper error message by error code (including extra codes ZOO_EXTRA_ERROR)
*/
std::string
get_error_message(int rc);

/**
* Get zookeper error message by error code (including extra codes ZOO_EXTRA_ERROR)
* and append exception message
*/
std::string
get_error_message(int rc, const std::exception& e);


/**
* Get nth parent of path (starting from 0)
* For path /A/B/C/D 0th parent is /A/B/C and 2nd parent is /A
*/
path_t
path_parent(const path_t& path, unsigned int depth);
}

#endif