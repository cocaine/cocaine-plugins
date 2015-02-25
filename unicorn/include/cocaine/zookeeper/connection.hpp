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

class cfg_t {
public:
    class endpoint_t {
    public:
        endpoint_t(std::string _hostname, unsigned int _port);

        std::string
        to_string() const;

    private:
        std::string hostname;
        unsigned int port;
    };

    cfg_t(std::vector<endpoint_t> endpoints, unsigned int recv_timeout);

    /**
    * ZK connection string.
    * ZK accepts several host:port values of a cluster splitted by comma.
    */
    std::string
    connection_string() const;

    const unsigned int recv_timeout;
private:
    std::vector<endpoint_t> endpoints;

};

/**
* Adapter class to zookeeper C api.
* Add ability to pass std::unique_ptr of handler object instead of function callback and void*
*/
class connection_t :
    public watch_handler_base_t
{
public:
    connection_t(const cfg_t& cfg, const session_t& session);

    ~connection_t();

    /**
    * forced put (no version check)
    * See zoo_aset.
    */
    void
    put(const path_t& path, const value_t& value, stat_handler_ptr handler);

    /**
    * put value to path. If version in ZK is different returns an error.
    * See zoo_aset.
    */
    void
    put(const path_t& path, const value_t& value, version_t version, stat_handler_ptr handler);

    /**
    * Get value from ZK.
    * See zoo_aget.
    */
    void
    get(const path_t& path, data_handler_ptr handler);

    /**
    * Get node value from ZK and set watch for that node.
    * See zoo_awget
    */
    void
    get(const path_t& path, data_handler_with_watch_ptr handler, watch_handler_ptr watch_handler);

    /**
    * Create node in ZK with specified path and value.
    * See zoo_acreate
    */
    void
    create(const path_t& path, const value_t& value, bool ephemeral, string_handler_ptr handler);

    /**
    * delete node in ZK
    * See zoo_adelete
    */
    void
    del(const path_t& path, version_t version, void_handler_ptr handler);

    /**
    * Check if node exists
    */
    void
    exists(const path_t& path, stat_handler_with_watch_ptr handler, watch_handler_ptr watch);

    /**
    * Get node value from ZK and set watch for that node.
    * See zoo_awget
    */
    void
    childs(const path_t& path, strings_stat_handler_with_watch_ptr handler, watch_handler_ptr watch_handler);

    virtual void operator()(int type, int state, path_t path);

    void reconnect();
private:
    cfg_t cfg;
    session_t session;
    zhandle_t* zhandle;
    void check_rc(int rc);
    void check_connectivity();

};
}
#endif