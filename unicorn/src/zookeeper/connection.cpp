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

template <class Ptr>
void* mc_ptr(Ptr p) {
    return reinterpret_cast<void*>(static_cast<managed_handler_base_t*>(p));
}

}

cfg_t::endpoint_t::endpoint_t(std::string _hostname, unsigned int _port) :
    hostname(std::move(_hostname)),
    port(_port)
{}

std::string
cfg_t::endpoint_t::to_string() const {
    if(hostname.empty() || port == 0) {
        throw std::system_error(cocaine::error::INVALID_CONNECTION_ENDPOINT);
    }
    //Zookeeper handles even ipv6 addresses correctly in this case
    return hostname + ':' + std::to_string(port);
}


cfg_t::cfg_t(std::vector<cfg_t::endpoint_t> _endpoints, unsigned int _recv_timeout_ms):
    recv_timeout_ms(_recv_timeout_ms),
    endpoints(std::move(_endpoints))
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
    zhandle(),
    w_scope(),
    watcher(w_scope.get_handler<reconnect_action_t>(*this))
{
    reconnect();
}

zookeeper::connection_t::~connection_t() {
    if(zhandle) {
        zookeeper_close(zhandle);
    }
}

void
connection_t::put(const path_t& path, const value_t& value, version_t version, managed_stat_handler_base_t& handler) {
    check_connectivity();
    check_rc(
        zoo_aset(zhandle, path.c_str(), value.c_str(), value.size(), version, &handler_dispatcher_t::stat_cb, mc_ptr(&handler))
    );
}

void
connection_t::get(const path_t& path, managed_data_handler_base_t& handler, managed_watch_handler_base_t& watch) {
    check_connectivity();
    check_rc(
        zoo_awget(zhandle, path.c_str(), &handler_dispatcher_t::watcher_cb, mc_ptr(&watch), &handler_dispatcher_t::data_cb, mc_ptr(&handler))
    );
}

void
connection_t::get(const path_t& path, managed_data_handler_base_t& handler) {
    check_connectivity();
    check_rc(
        zoo_awget(zhandle, path.c_str(), &handler_dispatcher_t::watcher_cb, nullptr, &handler_dispatcher_t::data_cb, mc_ptr(&handler))
    );
}

void
connection_t::create(const path_t& path, const value_t& value, bool ephemeral, bool sequence, managed_string_handler_base_t& handler) {
    check_connectivity();
    auto acl = ZOO_OPEN_ACL_UNSAFE;
    int flag = ephemeral ? ZOO_EPHEMERAL : 0;
    flag = flag | (sequence ? ZOO_SEQUENCE : 0);
    check_rc(
        zoo_acreate(zhandle, path.c_str(), value.c_str(), value.size(), &acl, flag, &handler_dispatcher_t::string_cb, mc_ptr(&handler))
    );
}

void
connection_t::del(const path_t& path, version_t version, std::unique_ptr<void_handler_base_t> handler) {
    check_connectivity();
    check_rc(
        zoo_adelete(zhandle, path.c_str(), version, &handler_dispatcher_t::void_cb, c_ptr(handler.get()))
    );
    handler.release();
}

void
connection_t::exists(const path_t& path, managed_stat_handler_base_t& handler, managed_watch_handler_base_t& watch) {
    check_connectivity();
    check_rc(
        zoo_awexists(zhandle, path.c_str(), &handler_dispatcher_t::watcher_cb, mc_ptr(&watch), &handler_dispatcher_t::stat_cb, mc_ptr(&handler))
    );
}

void
connection_t::childs(const path_t& path, managed_strings_stat_handler_base_t& handler, managed_watch_handler_base_t& watch) {
    check_connectivity();
    check_rc(
        zoo_awget_children2(zhandle, path.c_str(), &handler_dispatcher_t::watcher_cb, mc_ptr(&watch), &handler_dispatcher_t::strings_stat_cb, mc_ptr(&handler))
    );
}

void
connection_t::childs(const path_t& path, managed_strings_stat_handler_base_t& handler) {
    check_connectivity();
    check_rc(
        zoo_awget_children2(zhandle, path.c_str(), nullptr, nullptr, &handler_dispatcher_t::strings_stat_cb, mc_ptr(&handler))
    );
}

void connection_t::check_connectivity() {
    if(!zhandle || is_unrecoverable(zhandle)) {
        reconnect();
    }
}
void connection_t::check_rc(int rc) const {
    if(rc != ZOK) {
        auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
        throw std::system_error(code);
    }
}

void connection_t::reconnect() {

    zhandle_t* new_zhandle = init();
    if(!new_zhandle) {
        if(session.valid()) {
            //Try to reset session before second attempt
            session.reset();
            new_zhandle = init();
        }
        if(!new_zhandle || is_unrecoverable(new_zhandle)) {
            // Swap in any case.
            // Sometimes we really want to force reconnect even when zk is unavailable at all. For example on lock release.
            std::swap(new_zhandle, zhandle);
            throw std::system_error(cocaine::error::COULD_NOT_CONNECT);
        }
    } else {
        if(!session.valid()) {
            session.assign(*zoo_client_id(new_zhandle));
        }
    }
    std::swap(new_zhandle, zhandle);
    zookeeper_close(new_zhandle);
}

zhandle_t* connection_t::init() {
    zhandle_t* new_zhandle = zookeeper_init(
        cfg.connection_string().c_str(),
        &handler_dispatcher_t::watcher_cb,
        cfg.recv_timeout_ms,
        session.native(),
        &watcher,
        0
    );
    return new_zhandle;
}

void connection_t::reconnect_action_t::operator()(int type, int state, path_t /*path*/) {
    if(type == ZOO_SESSION_EVENT) {
        if(state == ZOO_EXPIRED_SESSION_STATE) {
            parent.session.reset();
            parent.reconnect();
        }
    }
}
}
