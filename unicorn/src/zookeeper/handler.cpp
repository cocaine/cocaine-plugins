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
void watcher_cb (zhandle_t* zh, int type, int state, const char* path, void* watcherCtx) {
    std::unique_ptr<watch_handler_base_t> ptr (static_cast<watch_handler_base_t*>(watcherCtx));
    ptr->operator()(type, state, path_t(path));
}

void void_cb(int rc, const void *data) {
    std::unique_ptr<void_handler_base_t> ptr = static_cast<void_handler_base_t*>(data);
    ptr->operator()(rc);
}

void stat_cb(int rc, const struct Stat *stat, const void *data) {
    std::unique_ptr<stat_handler_base_t> ptr = static_cast<stat_handler_base_t*>(data);
    ptr->operator()(rc, *stat);
}

void data_cb(int rc, const char *value, int value_len, const struct Stat *stat, const void *data) {
    std::unique_ptr<data_handler_base_t> ptr = static_cast<data_handler_base_t*>(data);
    ptr->operator()(rc, std::string(value, value_len), *stat);
}
/*
void string_cb(int rc, const char *value, const void *data);
void strings_cb(int rc, const struct String_vector *strings, const void *data);
void strings_stat_cb(int rc, const struct String_vector *strings, const struct Stat *stat, const void *data);
void acl_cb(int rc, struct ACL_vector *acl, struct Stat *stat, const void *data);
*/


void watch_handler_base_t::preprocess(int type, int state, path_t path) {}

void stat_handler_base_t::preprocess(int rc, const node_stat& stat) {}

void data_handler_base_t::preprocess(int rc, std::string value, const node_stat& stat) {}

create_and_put_handler_t::create_and_put_handler_t(std::string _path, std::string _value) :
    path(std::move(_path)),
    value(std::move(_value))
{}

void create_and_put_handler_t::preprocess(int rc, std::string value, const node_stat& stat) {
    if(rc == ZNONODE) {

    }
}