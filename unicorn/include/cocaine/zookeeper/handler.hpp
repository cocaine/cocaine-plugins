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
#ifndef ZOOKEEPER_HANDLER_HPP
#define ZOOKEEPER_HANDLER_HPP

#include "cocaine/zookeeper/zookeeper.hpp"

#include <functional>
#include <string>
#include <memory>
#include <vector>

namespace zookeeper {

typedef Stat node_stat;

void
watcher_cb(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx);

void
watcher_non_owning_cb(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx);

void
void_cb(int rc, const void *data);

void
stat_cb(int rc, const struct Stat *stat, const void *data);

void
stat_with_watch_cb(int rc, const struct Stat* stat, const void* data);

void
data_cb(int rc, const char *value, int value_len, const struct Stat *stat, const void *data);

void
data_with_watch_cb(int rc, const char *value, int value_len, const struct Stat *stat, const void *data);

void
string_cb(int rc, const char *value, const void *data);

void
strings_stat_cb(int rc, const struct String_vector *strings, const struct Stat *stat, const void *data);

void
strings_stat_with_watch_cb(int rc, const struct String_vector *strings, const struct Stat *stat, const void *data);

//void strings_cb(int rc, const struct String_vector *strings, const void *data);

//void acl_cb(int rc, struct ACL_vector *acl, struct Stat *stat, const void *data);


class watch_handler_base_t {
public:
    virtual void
    operator() (int type, int state, path_t path) = 0;

    virtual
    ~watch_handler_base_t() {}
};

class void_handler_base_t {
public:
    virtual void
    operator() (int rc) = 0;

    virtual
    ~void_handler_base_t() {}
};

class stat_handler_base_t {
public:
    virtual void
    operator() (int rc, const node_stat& stat) = 0;

    virtual
    ~stat_handler_base_t() {}
};

class stat_handler_with_watch_t {
public:
    stat_handler_with_watch_t();

    virtual
    ~stat_handler_with_watch_t() {}

    virtual void
    operator() (int rc, const node_stat& stat) = 0;
private:
    friend void
    stat_with_watch_cb(int rc, const struct Stat *stat, const void *data);

    friend class connection_t;

    void
    run (int rc, const node_stat& stat);

    void
    bind_watch(watch_handler_base_t* _watch_ptr);

    watch_handler_base_t* watch_ptr;
};

class data_handler_base_t {
public:
    virtual void
    operator() (int rc, value_t value, const node_stat& stat) = 0;

    virtual
    ~data_handler_base_t() {}
};

class data_handler_with_watch_t {
public:
    data_handler_with_watch_t();

    virtual
    ~data_handler_with_watch_t() {}

    virtual void
    operator() (int rc, value_t value, const node_stat& stat) = 0;

private:
    friend void
    data_with_watch_cb(int rc, const char *value, int value_len, const struct Stat *stat, const void *data);

    friend class connection_t;

    void
    run (int rc, value_t value, const node_stat& stat);

    void
    bind_watch(watch_handler_base_t* _watch_ptr);

    watch_handler_base_t* watch_ptr;
};

class string_handler_base_t {
public:
    virtual void
    operator() (int rc, value_t value) = 0;

    virtual
    ~string_handler_base_t() {}
};

class strings_stat_handler_base_t {
public:
    virtual void
    operator() (int rc, std::vector<std::string> childs, const node_stat& stat) = 0;

    virtual
    ~strings_stat_handler_base_t() {}
};

class strings_stat_handler_with_watch_t {
public:
    strings_stat_handler_with_watch_t();

    virtual
    ~strings_stat_handler_with_watch_t() {}

    virtual void
    operator() (int rc, std::vector<std::string> childs, const node_stat& stat) = 0;

private:
    friend void
    strings_stat_with_watch_cb(int rc, const struct String_vector *strings, const struct Stat *stat, const void *data);

    friend class connection_t;

    void
    run(int rc, std::vector<std::string> childs, const node_stat& stat);

    void
    bind_watch(watch_handler_base_t* _watch_ptr);

    watch_handler_base_t* watch_ptr;
};

typedef std::unique_ptr<watch_handler_base_t> watch_handler_ptr;
typedef std::unique_ptr<void_handler_base_t> void_handler_ptr;
typedef std::unique_ptr<stat_handler_base_t> stat_handler_ptr;
typedef std::unique_ptr<stat_handler_with_watch_t> stat_handler_with_watch_ptr;
typedef std::unique_ptr<data_handler_base_t> data_handler_ptr;
typedef std::unique_ptr<data_handler_with_watch_t> data_handler_with_watch_ptr;
typedef std::unique_ptr<string_handler_base_t> string_handler_ptr;
typedef std::unique_ptr<strings_stat_handler_base_t> strings_stat_handler_ptr;
typedef std::unique_ptr<strings_stat_handler_with_watch_t> strings_stat_handler_with_watch_ptr;
}
#endif