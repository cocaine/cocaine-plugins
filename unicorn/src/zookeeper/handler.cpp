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

#include "cocaine/zookeeper/handler.hpp"
#include "cocaine/zookeeper/zookeeper.hpp"

namespace zookeeper {
namespace {
template <class Target>
Target* back_cast(const void* data) {
    return const_cast<Target*>(static_cast<const Target*>(data));
}
}

void watcher_cb(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx) {
    if(watcherCtx != nullptr) {
        std::unique_ptr<watch_handler_base_t> ptr(static_cast<watch_handler_base_t*>(watcherCtx));
        ptr->operator()(type, state, path_t(path));
    }
}

void void_cb(int rc, const void* data) {
    if(data != nullptr) {
        std::unique_ptr<void_handler_base_t> ptr(back_cast<void_handler_base_t>(data));
        ptr->operator()(rc);
    }
}

void stat_cb(int rc, const struct Stat* stat, const void* data) {
    std::unique_ptr<stat_handler_base_t> ptr(back_cast<stat_handler_base_t>(data));
    ptr->operator()(rc, *stat);
}

void data_cb(int rc, const char* value, int value_len, const struct Stat* stat, const  void* data) {
    std::unique_ptr<data_handler_base_t> ptr(back_cast<data_handler_base_t>(data));
    ptr->operator()(rc, std::string(value, value_len), *stat);
}

void string_cb(int rc, const char *value, const void *data) {
    std::string s_value;
    if(value != nullptr) {
        s_value = value;
    }
    std::unique_ptr<string_handler_base_t> ptr(back_cast<string_handler_base_t>(data));
    ptr->operator()(rc, s_value);
}
/*
void strings_cb(int rc, const struct String_vector *strings, const void *data);
void strings_stat_cb(int rc, const struct String_vector *strings, const struct Stat *stat, const void *data);
void acl_cb(int rc, struct ACL_vector *acl, struct Stat *stat, const void *data);
*/
std::string get_error_message(int rc) {
    switch (rc) {
        case CHILD_NOT_ALLOWED :
            return "Can not get value of a node with childs";
        case INVALID_TYPE :
            return "Invalid type of value stored for requested operation";
        default:
            return zerror(rc);
    }
}
}
