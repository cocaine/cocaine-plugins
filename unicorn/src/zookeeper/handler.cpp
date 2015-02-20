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

/**
* All callback are called from C ZK client, convert previously passed void* to
* matching callback and invoke it.
*/

void watcher_cb(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx) {
    if(watcherCtx != nullptr) {
        std::unique_ptr<watch_handler_base_t> ptr(static_cast<watch_handler_base_t*>(watcherCtx));
        ptr->operator()(type, state, path ? path_t(path) : path_t());
    }
}

void watcher_non_owning_cb(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx) {
    watch_handler_base_t* ptr(static_cast<watch_handler_base_t*>(watcherCtx));
    ptr->operator()(type, state, path ? path_t(path) : path_t());
}

void void_cb(int rc, const void* data) {
    if(data != nullptr) {
        std::unique_ptr<void_handler_base_t> ptr(back_cast<void_handler_base_t>(data));
        ptr->operator()(rc);
    }
}

void stat_cb(int rc, const struct Stat* stat, const void* data) {
    std::unique_ptr<stat_handler_base_t> ptr(back_cast<stat_handler_base_t>(data));
    ptr->operator()(rc, rc == ZOK ? *stat : node_stat());
}

void stat_with_watch_cb(int rc, const struct Stat* stat, const void* data) {
    std::unique_ptr<stat_handler_with_watch_t> ptr(back_cast<stat_handler_with_watch_t>(data));
    ptr->run(rc, rc == ZOK ? *stat : node_stat());
}

void data_cb(int rc, const char* value, int value_len, const struct Stat* stat, const  void* data) {
    std::unique_ptr<data_handler_base_t> ptr(back_cast<data_handler_base_t>(data));
    ptr->operator()(
        rc,
        rc == ZOK ? std::string(value, value_len) : std::string(),
        rc == ZOK ? *stat : node_stat()
    );
}

void data_with_watch_cb(int rc, const char* value, int value_len, const struct Stat* stat, const  void* data) {
    std::unique_ptr<data_handler_with_watch_t> ptr(back_cast<data_handler_with_watch_t>(data));
    ptr->run(
        rc,
        rc == ZOK ? std::string(value, value_len) : std::string(),
        rc == ZOK ? *stat : node_stat()
    );
}

void string_cb(int rc, const char *value, const void *data) {
    std::unique_ptr<string_handler_base_t> ptr(back_cast<string_handler_base_t>(data));
    ptr->operator()(rc, value ? value : std::string());
}

void
strings_stat_cb(int rc, const struct String_vector *strings, const struct Stat *stat, const void *data) {
    strings_stat_handler_ptr ptr(back_cast<strings_stat_handler_base_t>(data));
    ptr->operator()(
        rc,
        rc == ZOK ? std::vector<std::string>(strings->data, strings->data+strings->count) : std::vector<std::string>(),
        rc == ZOK ? *stat : node_stat()
    );
}

void
strings_stat_with_watch_cb(int rc, const struct String_vector *strings, const struct Stat *stat, const void *data) {
    strings_stat_handler_with_watch_ptr ptr(back_cast<strings_stat_handler_with_watch_t>(data));
    ptr->run(
        rc,
        rc == ZOK ? std::vector<std::string>(strings->data, strings->data+strings->count) : std::vector<std::string>(),
        rc == ZOK ? *stat : node_stat()
    );
}

/*
void strings_cb(int rc, const struct String_vector *strings, const void *data);
void strings_stat_cb(int rc, const struct String_vector *strings, const struct Stat *stat, const void *data);
void acl_cb(int rc, struct ACL_vector *acl, struct Stat *stat, const void *data);
*/

data_handler_with_watch_t::data_handler_with_watch_t() :
    watch_ptr(nullptr)
{}

void
data_handler_with_watch_t::run (int rc, value_t value, const node_stat& stat) {
    if(rc != ZOK && watch_ptr) {
        delete watch_ptr;
    }
    operator()(rc, value, stat);
}

void
data_handler_with_watch_t::bind_watch(watch_handler_base_t* _watch_ptr) {
    watch_ptr = _watch_ptr;
}

stat_handler_with_watch_t::stat_handler_with_watch_t() :
    watch_ptr(nullptr)
{
}

void stat_handler_with_watch_t::run(int rc, const node_stat& stat) {
    //Seems that watch can be woken up in case of ZNONODE
    if(rc != ZOK && rc != ZNONODE && watch_ptr) {
        delete watch_ptr;
    }
    operator()(rc, stat);
}

void stat_handler_with_watch_t::bind_watch(watch_handler_base_t* _watch_ptr) {
    watch_ptr = _watch_ptr;
}

strings_stat_handler_with_watch_t::strings_stat_handler_with_watch_t():
    watch_ptr(nullptr)
{
}

void strings_stat_handler_with_watch_t::run(int rc, std::vector<std::string> childs, const node_stat& stat) {
    if(rc != ZOK) {
        delete watch_ptr;
    }
    operator()(rc, std::move(childs), stat);
}

void strings_stat_handler_with_watch_t::bind_watch(watch_handler_base_t* _watch_ptr) {
    watch_ptr = _watch_ptr;
}

}
