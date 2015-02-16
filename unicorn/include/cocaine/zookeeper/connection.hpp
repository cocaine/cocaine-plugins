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
#ifndef ZOOKEEPER_CONNECTION_HPP
#define ZOOKEEPER_CONNECTION_HPP

#include "cocaine/zookeeper/session.hpp"
#include "cocaine/zookeeper/handler.hpp"

#include <zookeeper/zookeeper.h>

#include <vector>
#include <string>

namespace zookeeper {

struct cfg_t {
    struct endpoint_t {
        std::string hostname;
        unsigned int port;
    };
    std::vector<endpoint_t> endpoints;
    unsigned int recv_timeout;
    cfg_t(std::vector<endpoint_t> endpoints, unsigned int recv_timeout);
    std::string connection_string() const;
};
class connection_t {
public:
    connection_t(const cfg_t& cfg, session_t& session);
    void put(const std::string& path, const std::string& value, stat_handler_ptr handler);
    void get(const std::string& path, data_handler_ptr handler);
    void get(const std::string& path, data_handler_ptr handler, watch_handler_ptr watch_handler);
private:
    zhandle_t* zhandle;
};
}
#endif