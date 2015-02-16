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
#include <AppKit/AppKit.h>


namespace zookeeper {
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

void connection_t::put(const std::string& path, const std::string& value, stat_handler_ptr handler) {
    auto ptr = handler.release();
    zoo_aset(zhandle, path.c_str(), value.c_str(), value.size(), -1, stat_cb, static_cast<void*>(handler));
}

void connection_t::get(const std::string& path, data_handler_ptr handler) {
    auto ptr = handler.release();
    zoo_aget(zhandle, path.c_str(), -1, data_cb, static_cast<void*>(handler));
}

void connection_t::get(const std::string& path, data_handler_ptr handler, watch_handler_ptr watch) {
    zoo_awget(zhandle, path.c_str(), watcher_cb, static_cast<void*>(watch.release()), data_cb, static_cast<void*>(handler.release()));
}


}
