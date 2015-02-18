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
        endpoint_t(std::string _hostname, unsigned int _port) :
            hostname(_hostname),
            port(_port)
        {}
        std::string hostname;
        unsigned int port;
        std::string to_string() const {
            std::string result;
            if(!hostname.empty() && port != 0) {
                result = hostname + ':' + std::to_string(port);
            }
            return result;
        }
    };
    std::vector<endpoint_t> endpoints;
    unsigned int recv_timeout;
    cfg_t(std::vector<endpoint_t> endpoints, unsigned int recv_timeout);
    std::string connection_string() const;
};

class connection_t {
public:
    connection_t(const cfg_t& cfg, session_t& session);
    void put(const path_t& path, const value_t& value, stat_handler_ptr handler);
    void put(const path_t& path, const value_t& value, version_t version, stat_handler_ptr handler);
    void get(const path_t& path, data_handler_ptr handler);
    void get(const path_t& path, data_handler_ptr handler, watch_handler_ptr watch_handler);
    void create(const path_t& path, const value_t& value, string_handler_ptr handler);
    void del(const path_t& path, version_t version, void_handler_ptr handler);
private:
    zhandle_t* zhandle;
};
}
#endif