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
#include "cocaine/zookeeper/exception.hpp"

#include <cocaine/locked_ptr.hpp>

#include <functional>
#include <string>
#include <memory>
#include <vector>

#include <iostream>
#include <atomic>
#include <unordered_map>

namespace zookeeper {

typedef Stat node_stat;

class handler_tag {
private:
    handler_tag() {}
    friend class handler_scope_t;
};

class managed_handler_base_t {
public:
    // Tagged constructor.
    // Used to disable implicit instantiation.
    // All instantiations are made through scope object.
    managed_handler_base_t(const handler_tag&) {}
    managed_handler_base_t(const managed_handler_base_t& other) = delete;
    managed_handler_base_t& operator=(const managed_handler_base_t& other) = delete;
    virtual ~managed_handler_base_t() {}
};

class handler_dispatcher_t {
public:
    static
    void
        watcher_cb(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx);

    static
    void
        void_cb(int rc, const void *data);

    static
    void
        stat_cb(int rc, const struct Stat *stat, const void *data);

    static
    void
        data_cb(int rc, const char *value, int value_len, const struct Stat *stat, const void *data);

    static
    void
        managed_data_cb(int rc, const char *value, int value_len, const struct Stat *stat, const void *data);

    static
    void
        string_cb(int rc, const char *value, const void *data);

    static
    void
        strings_stat_cb(int rc, const struct String_vector *strings, const struct Stat *stat, const void *data);

private:
    typedef std::shared_ptr<managed_handler_base_t> handler_ptr;

    //Ugly hack. That is fixed in C++14 - we can search in set by convertible value. Now we use map.
    typedef std::unordered_map<managed_handler_base_t*, handler_ptr> storage_t;

    friend
    class handler_scope_t;

    static handler_dispatcher_t& instance();

    template<class T, class ...Args>
    void
    call(managed_handler_base_t* callback, Args&& ...args) {
        std::shared_ptr<T> cb(nullptr);
        {
            std::unique_lock<std::mutex> lock(storage_lock);
            auto it = callbacks.find(callback);
            if (it != callbacks.end()) {
                cb = std::dynamic_pointer_cast<T>(it->second);
            }
        }
        if(cb) {
            std::cout << "CALL: " << callback << std::endl;
            cb->operator()(std::forward<Args>(args)...);
        }
    }

    void add(managed_handler_base_t* callback);

    void release(managed_handler_base_t* callback);

    handler_dispatcher_t() {}
    handler_dispatcher_t(const handler_dispatcher_t& other) = delete;
    handler_dispatcher_t& operator=(const handler_dispatcher_t& other) = delete;
    std::mutex storage_lock;
    storage_t callbacks;
};

class handler_scope_t {
public:
    handler_scope_t() :
        storage_mutex(),
        registered_callbacks()
    {}

    ~handler_scope_t();

    template<class T, class ...Args>
    T&
    get_handler(Args&& ...args) {
        auto handler = new T(handler_tag(), std::forward<Args>(args)...);
        handler_dispatcher_t::instance().add(handler);
        std::unique_lock<std::mutex> lock(storage_mutex);
        registered_callbacks.push_back(handler);
        return *handler;
    }

private:
    std::mutex storage_mutex;
    std::vector<managed_handler_base_t*> registered_callbacks;
};

class managed_watch_handler_base_t :
    virtual public managed_handler_base_t
{
public:
    virtual void
        operator() (int type, int state, path_t path) = 0;

    managed_watch_handler_base_t(const handler_tag& tag) :
        managed_handler_base_t(tag)
    {}
};

class void_handler_base_t {
public:
    virtual void
    operator() (int rc) = 0;

    virtual
    ~void_handler_base_t() {}
};

class managed_stat_handler_base_t :
    virtual public managed_handler_base_t
{
public:
    virtual void
    operator() (int rc, const node_stat& stat) = 0;

    managed_stat_handler_base_t(const handler_tag& tag) :
        managed_handler_base_t(tag)
    {}

    virtual
    ~managed_stat_handler_base_t() {}
};

class managed_data_handler_base_t :
    virtual public managed_handler_base_t
{
public:
    virtual void
    operator() (int rc, value_t value, const node_stat& stat) = 0;

    managed_data_handler_base_t(const handler_tag& tag) :
        managed_handler_base_t(tag)
    {}

    virtual
    ~managed_data_handler_base_t() {}
};

class managed_string_handler_base_t :
public managed_handler_base_t
{
public:
    virtual void
    operator() (int rc, value_t value) = 0;

    managed_string_handler_base_t(const handler_tag& tag) :
        managed_handler_base_t(tag)
    {}

    virtual
    ~managed_string_handler_base_t() {}
};

class managed_strings_stat_handler_base_t:
virtual public managed_handler_base_t
{
public:
    virtual void
        operator() (int rc, std::vector<std::string> childs, const node_stat& stat) = 0;

    managed_strings_stat_handler_base_t(const handler_tag& tag) :
        managed_handler_base_t(tag)
    {}
};


typedef std::unique_ptr<void_handler_base_t> void_handler_ptr;

}
#endif