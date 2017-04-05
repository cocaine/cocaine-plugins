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

#pragma once

#include <string>

namespace zookeeper {

typedef std::string path_t;
typedef std::string value_t;
typedef long long version_t;

/**
* Get nth parent of path (starting from 1)
* For path /A/B/C/D 1th parent is /A/B/C
*/
path_t
path_parent(const path_t& path, unsigned int depth);

/**
* Check if node has a sequence on it's end
*/
bool is_valid_sequence_node(const path_t& path);

/**
* Get sequence number from nodes created via automated sequence
*/
unsigned long
get_sequence_from_node_name_or_path(const path_t& path);

std::string
get_node_name(const path_t& path);

}
