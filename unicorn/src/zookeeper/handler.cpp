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

#include "cocaine/detail/zookeeper/handler.hpp"
#include "cocaine/detail/zookeeper/zookeeper.hpp"

#include <iostream>

namespace zookeeper {
namespace {
template <class Target>
Target* back_cast(const void* data) {
    return const_cast<Target*>(static_cast<const Target*>(data));
}

std::atomic<size_t> callback_counter;
}

managed_handler_base_t::managed_handler_base_t(const handler_tag&) :
    idx(callback_counter++)
{}

handler_dispatcher_t::handler_dispatcher_t() :
    storage_lock(),
    callbacks(),
    active(true)
{}
/**
* All callback are called from C ZK client, convert previously passed void* to
* matching callback and invoke it.
*/

handler_dispatcher_t::~handler_dispatcher_t() {
    decltype(callbacks) tmp_callbacks;
    {
        std::unique_lock<std::mutex> lock(storage_lock);
        active = false;
        tmp_callbacks.swap(callbacks);
    }
    tmp_callbacks.clear();
}

void
handler_dispatcher_t::watcher_cb(zhandle_t* /*zh*/, int type, int state, const char* path, void* watcherCtx) {
    if(!watcherCtx) {
        return;
    }
    handler_dispatcher_t::instance().call<managed_watch_handler_base_t>(
        watcherCtx,
        type,
        state,
        path
    );
}

void
handler_dispatcher_t::void_cb(int rc, const void* data) {
    if(data == nullptr) {
        return;
    }
    std::unique_ptr<void_handler_base_t> ptr(back_cast<void_handler_base_t>(data));
    ptr->operator()(rc);
}

void
handler_dispatcher_t::stat_cb(int rc, const struct Stat* stat, const void* data) {
    if(data == nullptr) {
        return;
    }
    handler_dispatcher_t::instance().call<managed_stat_handler_base_t>(
        data,
        rc,
        rc == ZOK ? *stat : node_stat()
    );
}

void
handler_dispatcher_t::data_cb(int rc, const char* value, int value_len, const struct Stat* stat, const  void* data) {
    if(data == nullptr) {
        return;
    }
    handler_dispatcher_t::instance().call<managed_data_handler_base_t>(
        data,
        rc,
        rc == ZOK ? std::string(value, value_len) : std::string(),
        rc == ZOK ? *stat : node_stat()
    );
}

void
handler_dispatcher_t::string_cb(int rc, const char *value, const void *data) {
    if(data == nullptr) {
        return;
    }
    handler_dispatcher_t::instance().call<managed_string_handler_base_t>(
        data,
        rc,
        value ? value : std::string()
    );
}

void
handler_dispatcher_t::strings_stat_cb(int rc, const struct String_vector *strings, const struct Stat *stat, const void *data) {
    if(data == nullptr) {
        return;
    }
    handler_dispatcher_t::instance().call<managed_strings_stat_handler_base_t>(
        data,
        rc,
        rc == ZOK ? std::vector<std::string>(strings->data, strings->data+strings->count) : std::vector<std::string>(),
        rc == ZOK ? *stat : node_stat()
    );
}

handler_dispatcher_t& handler_dispatcher_t::instance() {
    static handler_dispatcher_t inst;
    return inst;
}

void handler_dispatcher_t::add(managed_handler_base_t* callback) {
    std::unique_lock<std::mutex> lock(storage_lock);
    if(active) {
        callbacks.insert(std::make_pair(callback->get_id(), handler_ptr(callback)));
    }
}

void handler_dispatcher_t::release(managed_handler_base_t* callback) {
    std::unique_lock<std::mutex> lock(storage_lock);
    auto it = callbacks.find(callback->get_id());
    if(it != callbacks.end()) {
        callbacks.erase(it);
    } else {
        std::cerr << "could not release managed callback - not found\n";
    }
}

handler_scope_t::~handler_scope_t() {
    for(size_t i = 0; i < registered_callbacks.size(); i++) {
        handler_dispatcher_t::instance().release(registered_callbacks[i]);
    }
}

void
managed_string_handler_base_t::operator() (int rc, zookeeper::path_t value) {
    if(!rc) {
        assert(value.size() > prefix.size());
        // TODO: Heavy assertion. Maybe we should delete it as we use asserts in prod.
        assert(value.substr(0, prefix.size()) == prefix);
        value.erase(0, prefix.size());
    }
    string_event(rc, std::move(value));
}

}
