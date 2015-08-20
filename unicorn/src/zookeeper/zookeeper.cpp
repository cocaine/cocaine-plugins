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

#include "cocaine/unicorn/errors.hpp"

#include "cocaine/zookeeper/zookeeper.hpp"

#include <stdexcept>
#include <errno.h>

namespace zookeeper {

/*
std::string
get_error_message(int rc) {
    switch (rc) {
        case CHILD_NOT_ALLOWED :
            return "Can not get value of a node with childs";
        case INVALID_TYPE :
            return "Invalid type of value stored for requested operation";
        case INVALID_VALUE :
            return "Could not unserialize value stored in zookeeper";
        case COULD_NOT_CONNECT:
            return "Could not reconnect to zookeper. Current errno:" + std::to_string(errno);
        case UNKNOWN_ERROR:
            return "Unknown zookeeper error";
        case HANDLER_SCOPE_RELEASED:
            return "Handler scope was released";
        case INVALID_NODE_NAME:
            return "Inavlid node name specified";
        case INVALID_PATH:
            return "Inavlid path specified";
        case VERSION_NOT_ALLOWED:
            return "Specified version is not allowed for command";
        case INVALID_CONNECTION_ENDPOINT:
            return "Invalid connection endpoint specified";
        default:
            return zerror(rc);
    }
}

std::string get_error_message(int rc, const std::exception& e) {
    return get_error_message(rc) + ", exception: " + e.what();
}
*/

path_t
path_parent(const path_t& path, unsigned int depth) {
    if(depth == 0) {
        return path;
    }
    size_t last_char = path.size();
    size_t pos = std::string::npos;
    for (size_t i = 0; i < depth; i++) {
        pos = path.find_last_of('/', last_char);
        if (pos == path.size() - 1) {
            last_char = pos - 1;
            pos = path.find_last_of('/', last_char);
        }
        if (pos == std::string::npos || pos == 0) {
            throw std::runtime_error("could not get " + std::to_string(depth) + " level parent from path: " + path);
        }
        last_char = pos - 1;
    }
    return path.substr(0, pos);
}

bool is_valid_sequence_node(const path_t& path) {
    return !path.empty() && isdigit(static_cast<unsigned char>(path[path.size()-1]));
}

unsigned long
get_sequence_from_node_name_or_path(const path_t& path) {
    if(!is_valid_sequence_node(path)) {
        throw std::system_error(cocaine::error::INVALID_NODE_NAME);
    }
    auto pos = path.size()-1;
    unsigned char ch = static_cast<unsigned char>(path[pos]);
    while(isdigit(ch)) {
        pos--;
        ch =  static_cast<unsigned char>(path[pos]);
    }
    pos++;
    return std::stoul(path.substr(pos));
}

std::string
get_node_name(const path_t& path) {
    auto pos = path.find_last_of('/');
    if(pos == std::string::npos || pos == path.size()-1) {
        throw std::system_error(cocaine::error::INVALID_PATH);
    }
    return path.substr(pos+1);
}

}
