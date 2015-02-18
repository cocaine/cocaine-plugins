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

#include "cocaine/zookeeper/connection.hpp"
#include "cocaine/zookeeper/session.hpp"
#include "cocaine/zookeeper/handler.hpp"
#include <zookeeper/zookeeper.h>
#include <stdexcept>
#include <string>
#include <errno.h>

namespace zookeeper {
namespace {
    void check_rc(int rc) {
        if(rc != ZOK) {
            throw std::runtime_error("Zookeeper connection error " + std::to_string(rc) + " : " + get_error_message(rc));
        }
    }
    template <class Ptr>
    void* c_ptr(Ptr p) {
        return reinterpret_cast<void*>(p);
    }
}
zookeeper::connection_t::connection_t(const cfg_t& cfg, session_t& session) :
    zhandle(zookeeper_init(cfg.connection_string().c_str(), watcher_cb, cfg.recv_timeout, session.native(), nullptr, 0))
{
    if(!zhandle) {
        if(session.valid()) {
            session.reset();
            zhandle = zookeeper_init(cfg.connection_string().c_str(), watcher_cb, cfg.recv_timeout, session.native(), nullptr, 0);
        }
        if(!zhandle) {
            throw std::runtime_error("Could not connect to zookeper. Errno: " + std::to_string(errno));
        }
    }
    else {
        if(!session.valid()) {
            session.assign(zoo_client_id(zhandle));
        }
    }
}

void connection_t::put(const path_t& path, const value_t& value, stat_handler_ptr handler) {
    check_rc(
        zoo_aset(zhandle, path.c_str(), value.c_str(), value.size(), -1, stat_cb, c_ptr(handler.release()))
    );
}

void connection_t::put(const path_t& path, const value_t& value, version_t version, stat_handler_ptr handler) {
    check_rc(
        zoo_aset(zhandle, path.c_str(), value.c_str(), value.size(), version, stat_cb, c_ptr(handler.release()))
    );
}

void connection_t::get(const path_t& path, data_handler_ptr handler) {
    check_rc(
        zoo_aget(zhandle, path.c_str(), -1, data_cb, c_ptr(handler.release()))
    );
}

void connection_t::get(const path_t& path, data_handler_ptr handler, watch_handler_ptr watch) {
    check_rc(
        zoo_awget(zhandle, path.c_str(), watcher_cb, c_ptr(watch.release()), data_cb, c_ptr(handler.release()))
    );
}

void connection_t::create(const path_t& path, const value_t& value, string_handler_ptr handler) {
    auto acl = ZOO_OPEN_ACL_UNSAFE;
    check_rc(
        zoo_acreate(zhandle, path.c_str(), value.c_str(), value.size(), &acl, 0, string_cb, c_ptr(handler.release()))
    );
}

void connection_t::del(const path_t& path, version_t version, void_handler_ptr handler) {
    check_rc(
        zoo_adelete(zhandle, path.c_str(), version, void_cb, c_ptr(handler.release()))
    );
}

cfg_t::cfg_t(std::vector<cfg_t::endpoint_t> _endpoints, unsigned int _recv_timeout):
    endpoints(std::move(_endpoints)),
    recv_timeout(_recv_timeout)
{
}

std::string cfg_t::connection_string() const {
    std::string result;
    for(size_t i = 0; i < endpoints.size(); i++) {
        if(!result.empty()) {
            result += ',';
        }
        result += endpoints[i].to_string();
    }
    return result;
}

}
