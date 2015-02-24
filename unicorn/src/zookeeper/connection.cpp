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
#include "cocaine/zookeeper/exception.hpp"
#include <zookeeper/zookeeper.h>
#include <stdexcept>
#include <string>
#include <errno.h>

namespace zookeeper {
namespace {
template <class Ptr>
void* c_ptr(Ptr p) {
    return reinterpret_cast<void*>(p);
}
}

cfg_t::endpoint_t::endpoint_t(std::string _hostname, unsigned int _port) :
    hostname(std::move(_hostname)),
    port(_port)
{}

std::string
cfg_t::endpoint_t::to_string() const {
    std::string result;
    if(!hostname.empty() && port != 0) {
        result = hostname + ':' + std::to_string(port);
    }
    return result;
}


cfg_t::cfg_t(std::vector<cfg_t::endpoint_t> _endpoints, unsigned int _recv_timeout):
    endpoints(std::move(_endpoints)),
    recv_timeout(_recv_timeout)
{
}

std::string
cfg_t::connection_string() const {
    std::string result;
    for(size_t i = 0; i < endpoints.size(); i++) {
        if(!result.empty()) {
            result += ',';
        }
        result += endpoints[i].to_string();
    }
    return result;
}

zookeeper::connection_t::connection_t(const cfg_t& _cfg, const session_t& _session) :
    cfg(_cfg),
    session(_session),
    zhandle()
{
    reconnect();
}

void
connection_t::put(const path_t& path, const value_t& value, stat_handler_ptr handler) {
    check_connectivity();
    check_rc(
        zoo_aset(zhandle, path.c_str(), value.c_str(), value.size(), -1, stat_cb, c_ptr(handler.get()))
    );
    handler.release();
}

void
connection_t::put(const path_t& path, const value_t& value, version_t version, stat_handler_ptr handler) {
    check_connectivity();
    check_rc(
        zoo_aset(zhandle, path.c_str(), value.c_str(), value.size(), version, stat_cb, c_ptr(handler.get()))
    );
    handler.release();
}

void
connection_t::get(const path_t& path, data_handler_ptr handler) {
    check_connectivity();
    check_rc(
        zoo_aget(zhandle, path.c_str(), -1, data_cb, c_ptr(handler.get()))
    );
    handler.release();
}

void
connection_t::get(const path_t& path, data_handler_with_watch_ptr handler, watch_handler_ptr watch) {
    check_connectivity();
    handler->bind_watch(watch.get());
    check_rc(
        zoo_awget(zhandle, path.c_str(), watcher_cb, c_ptr(watch.get()), data_with_watch_cb, c_ptr(handler.get()))
    );
    handler.release();
    watch.release();
}

void
connection_t::create(const path_t& path, const value_t& value, bool ephemeral, string_handler_ptr handler) {
    check_connectivity();
    auto acl = ZOO_OPEN_ACL_UNSAFE;
    int flag = ephemeral ? ZOO_EPHEMERAL : 0;
    check_rc(
        zoo_acreate(zhandle, path.c_str(), value.c_str(), value.size(), &acl, flag, string_cb, c_ptr(handler.get()))
    );
    handler.release();
}

void
connection_t::del(const path_t& path, version_t version, void_handler_ptr handler) {
    check_connectivity();
    check_rc(
        zoo_adelete(zhandle, path.c_str(), version, void_cb, c_ptr(handler.get()))
    );
    handler.release();
}

void
connection_t::exists(const path_t& path, stat_handler_with_watch_ptr handler, watch_handler_ptr watch) {
    check_connectivity();
    handler->bind_watch(watch.get());
    check_rc(
        zoo_awexists(zhandle, path.c_str(), watcher_cb, c_ptr(watch.get()), stat_with_watch_cb, c_ptr(handler.get()))
    );
    handler.release();
    watch.release();
}

void
connection_t::childs(const path_t& path, strings_stat_handler_with_watch_ptr handler, watch_handler_ptr watch) {
    check_connectivity();
    handler->bind_watch(watch.get());
    check_rc(
        zoo_awget_children2(zhandle, path.c_str(), watcher_cb, c_ptr(watch.get()), strings_stat_with_watch_cb, c_ptr(handler.get()))
    );
    handler.release();
    watch.release();

}

void connection_t::check_connectivity() {
    if(is_unrecoverable(zhandle)) {
        reconnect();
    }
}
void connection_t::check_rc(int rc) {
    if(rc != ZOK) {
        throw exception("Zookeeper connection error. ", rc);
    }
}

void connection_t::operator()(int type, int state, path_t path) {
    if(type == ZOO_SESSION_EVENT) {
        if(state == ZOO_EXPIRED_SESSION_STATE) {
            session.reset();
            reconnect();
        }
    }
}

void connection_t::reconnect() {

    zhandle_t* new_zhandle = zookeeper_init(cfg.connection_string().c_str(), watcher_non_owning_cb, cfg.recv_timeout, session.native(), this, 0);
    if(!new_zhandle) {
        if(session.valid()) {
            //Try to reset session before second attempt
            session.reset();
            new_zhandle = zookeeper_init(cfg.connection_string().c_str(), watcher_non_owning_cb, cfg.recv_timeout, session.native(), this, 0);
        }
        if(!new_zhandle || is_unrecoverable(new_zhandle)) {
            throw exception(ZOO_EXTRA_ERROR::COULD_NOT_CONNECT);
        }
    }
    else {
        if(!session.valid()) {
            session.assign(*zoo_client_id(new_zhandle));
        }
    }
    std::swap(new_zhandle, zhandle);
    zookeeper_close(new_zhandle);
}

}
