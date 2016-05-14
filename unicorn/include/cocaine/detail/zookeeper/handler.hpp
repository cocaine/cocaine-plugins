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

#include "cocaine/detail/zookeeper/zookeeper.hpp"

#include <cocaine/locked_ptr.hpp>

#include <zookeeper/zookeeper.h>

#include <functional>
#include <string>
#include <memory>
#include <vector>

#include <unordered_map>
#include <cassert>

namespace zookeeper {

typedef Stat node_stat;

//Used to prevent direct creation of managed handlers derived classes.
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
    managed_handler_base_t(const handler_tag&);
    managed_handler_base_t(const managed_handler_base_t& other) = delete;
    managed_handler_base_t& operator=(const managed_handler_base_t& other) = delete;
    virtual
    ~managed_handler_base_t() {}

    size_t
    get_id() {
        return idx;
    }

private:
    size_t idx;
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
    stat_cb(int rc, const struct Stat* stat, const void* data);

    static
    void
    data_cb(int rc, const char* value, int value_len, const struct Stat* stat, const void* data);

    static
    void
    string_cb(int rc, const char* value, const void* data);

    static
    void
    strings_stat_cb(int rc, const struct String_vector* strings, const struct Stat* stat, const void* data);

private:
    typedef std::shared_ptr<managed_handler_base_t> handler_ptr;

    typedef std::unordered_map<size_t, handler_ptr> storage_t;

    friend
    class handler_scope_t;

    static handler_dispatcher_t& instance();

    template<class T, class ...Args>
    void
    call(const void* callback, Args&& ...args) {
        handler_ptr cb;
        size_t idx = reinterpret_cast<size_t>(callback);
        {
            std::lock_guard<std::mutex> lock(storage_lock);
            auto it = callbacks.find(idx);
            if (it != callbacks.end()) {
                cb = it->second;
                assert(cb);
            }
        }
        if(cb) {
            std::dynamic_pointer_cast<T>(cb)->operator()(std::forward<Args>(args)...);
        }
    }

    void add(managed_handler_base_t* callback);

    void release(managed_handler_base_t* callback);

    handler_dispatcher_t();
    ~handler_dispatcher_t();
    handler_dispatcher_t(const handler_dispatcher_t& other) = delete;
    handler_dispatcher_t& operator=(const handler_dispatcher_t& other) = delete;
    std::mutex storage_lock;
    storage_t callbacks;
    bool active;
};

/**
* Scope of the handlers. After this goes out of scope all handlers ae destroyed.
* However call of this handlers via handler manager is defined and do nothing.
* Thread unsafe. For thread safety consider external locking or creating separate class.
*/
class handler_scope_t {
public:
    handler_scope_t() :
        registered_callbacks()
    {}

    ~handler_scope_t();

    template<class T, class ...Args>
    T&
    get_handler(Args&& ...args) {
        auto handler = new T(handler_tag(), std::forward<Args>(args)...);
        handler_dispatcher_t::instance().add(handler);
        registered_callbacks.push_back(handler);
        return *handler;
    }

    template<class T>
    void
    release_handler(T& handler) {
        auto it = std::remove(registered_callbacks.begin(),registered_callbacks.end(), &handler);
        if(it == registered_callbacks.end()) {
            throw std::runtime_error("Specified callback not found in scope");
        }
        assert(it+1 == registered_callbacks.end());
        registered_callbacks.resize(it - registered_callbacks.begin());
        handler_dispatcher_t::instance().release(&handler);
    }

private:
    std::vector<managed_handler_base_t*> registered_callbacks;
};

class managed_watch_handler_base_t :
    virtual public managed_handler_base_t
{
public:
    void
    operator() (int type, int state, path_t path) {
        watch_event(type, state, std::move(path));
    }

    virtual
    void watch_event(int type, int state, path_t path)= 0;

    managed_watch_handler_base_t(const handler_tag& tag) :
        managed_handler_base_t(tag)
    {}
};

/**
* At this time managed void handler is unneeded so it's being deleted after invocation.
* Maybe better to stick to only managed handlers for consistency.
*/
class void_handler_base_t {
public:
    void
    operator() (int rc){
        void_event(rc);
    }

    virtual void
    void_event(int rc) = 0;

    virtual
    ~void_handler_base_t() {}
};

class managed_stat_handler_base_t :
    virtual public managed_handler_base_t
{
public:
    void
    operator() (int rc, const node_stat& stat) {
        stat_event(rc, stat);
    }

    virtual void
    stat_event(int rc, const node_stat& stat) = 0;

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
    operator() (int rc, value_t value, const node_stat& stat) {
        data_event(rc, std::move(value), stat);
    }

    virtual void
    data_event(int rc, value_t value, const node_stat& stat) = 0;

    managed_data_handler_base_t(const handler_tag& tag) :
        managed_handler_base_t(tag)
    {}

    virtual
    ~managed_data_handler_base_t() {}
};

class managed_string_handler_base_t :
    virtual public managed_handler_base_t
{
public:
    virtual void
    operator() (int rc, zookeeper::value_t value);

    virtual void
    string_event(int rc, zookeeper::value_t value) = 0;

    managed_string_handler_base_t(const handler_tag& tag) :
        managed_handler_base_t(tag)
    {}

    virtual
    ~managed_string_handler_base_t() {}

    void
    set_prefix(path_t _prefix) {
        prefix = std::move(_prefix);
    }
private:
    path_t prefix;
};

class managed_strings_stat_handler_base_t:
    virtual public managed_handler_base_t
{
public:
    void
    operator() (int rc, std::vector<std::string> childs, const node_stat& stat) {
        children_event(rc, std::move(childs), stat);
    }

    virtual void
    children_event(int rc, std::vector<std::string> childs, const node_stat& stat) = 0;

    managed_strings_stat_handler_base_t(const handler_tag& tag) :
        managed_handler_base_t(tag)
    {}
};


}
