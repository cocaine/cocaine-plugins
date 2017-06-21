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

#include <cocaine/api/unicorn.hpp>

#include <zookeeper/zookeeper.h>

#include <string>
#include <system_error>

namespace cocaine {
namespace zookeeper {

typedef std::string path_t;
typedef long long version_t;
typedef Stat stat_t;

/**
* Get nth parent of path (starting from 1)
* For path /A/B/C/D 1th parent is /A/B/C
*/
auto path_parent(const path_t& path, unsigned int depth) -> path_t;

/**
* Check if node has a sequence on it's end
*/
auto is_valid_sequence_node(const path_t& path) -> bool;

auto get_node_name(const path_t& path) -> std::string;

/**
* Serializes service representation of value to zookepeers representation.
* Currently ZK store msgpacked data, and service uses cocaine::dynamic_t
*/
auto serialize(const unicorn::value_t& val) -> std::string;

auto unserialize(const std::string& val) -> unicorn::value_t;

auto map_zoo_error(int rc) -> std::error_code;

auto event_to_string(int event) -> std::string;

auto state_to_string(int state) -> std::string;

} // namespace zookeeper
} // namespace cocaine
