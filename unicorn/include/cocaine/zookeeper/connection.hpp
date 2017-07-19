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

#include "cocaine/zookeeper.hpp"
#include "cocaine/zookeeper/session.hpp"

#include <cocaine/locked_ptr.hpp>
#include <cocaine/api/executor.hpp>

#include <boost/optional/optional.hpp>

#include <zookeeper/zookeeper.h>

#include <map>
#include <vector>
#include <string>
#include <thread>

namespace cocaine {
namespace zookeeper {

class cfg_t {
public:
    class endpoint_t {
    public:
        endpoint_t(std::string _hostname, unsigned int _port);

        auto to_string() const -> std::string;

    private:
        std::string hostname;
        unsigned int port;
    };

    cfg_t(std::vector<endpoint_t> endpoints, unsigned int recv_timeout_ms, std::string prefix);

    auto connection_string() const -> std::string;

    const unsigned int recv_timeout_ms;
    std::string prefix;
    std::vector<endpoint_t> endpoints;
};

struct put_reply_t {
    int rc;
    const stat_t& stat;
};

struct get_reply_t {
    int rc;
    std::string data;
    const stat_t& stat;
};

struct watch_reply_t {
    int type;
    int state;
    path_t path;
};

struct create_reply_t {
    int rc;
    path_t created_path;
};

struct del_reply_t {
    int rc;
};

struct exists_reply_t {
    int rc;
    const stat_t& stat;
};

struct children_reply_t {
    int rc;
    std::vector<std::string> children;
    const stat_t& stat;
};

template<class T>
struct replier {
    virtual
    auto operator()(T reply) -> void = 0;

    virtual
    ~replier(){}
};

template<>
struct replier<create_reply_t> {
    virtual
    auto operator()(create_reply_t reply) -> void = 0;

    virtual
    ~replier(){}

    std::string prefix;
};

template<class... Args>
using replier_ptr = std::shared_ptr<replier<Args...>>;

/**
* Adapter class to zookeeper C api.
* Add ability to pass std::unique_ptr of handler object instead of function callback and void*
*/
class connection_t {
public:
    using zhandle_ptr = std::shared_ptr<zhandle_t>;
    using watchers_t = std::map<size_t, replier_ptr<watch_reply_t>>;

    connection_t(const cfg_t& cfg, const session_t& session);
    connection_t(const connection_t&) = delete;
    connection_t& operator=(const connection_t&) = delete;

    auto put(const path_t& path, const std::string& value, version_t version, replier_ptr<put_reply_t> handler) -> void;

    auto get(const path_t& path, replier_ptr<get_reply_t> handler) -> void;
    auto get(const path_t& path, replier_ptr<get_reply_t> handler, replier_ptr<watch_reply_t> watcher) -> void;

    auto create(const path_t& path, const std::string& value, bool ephemeral, bool sequence, replier_ptr<create_reply_t> handler) -> void;

    auto del(const path_t& path, version_t version, replier_ptr<del_reply_t> handler) -> void;
    auto del(const path_t& path, replier_ptr<del_reply_t> handler) -> void;

    auto exists(const path_t& path, replier_ptr<exists_reply_t> handler) -> void;
    auto exists(const path_t& path, replier_ptr<exists_reply_t> handler, replier_ptr<watch_reply_t> watcher) -> void;

    auto childs(const path_t& path, replier_ptr<children_reply_t> handler) -> void;
    auto childs(const path_t& path, replier_ptr<children_reply_t> handler, replier_ptr<watch_reply_t> watcher) -> void;

    auto reconnect() -> void;

private:
    friend auto watch_cb(zhandle_t* zh, int type, int state, const char* path, void* watch_data) -> void;

    template<class ZooFunction, class Replier, class CCallback, class... Args>
    auto zoo_command(ZooFunction f, const path_t& path, Replier&& replier, CCallback cb, Args&&... args) -> void;

    template<class ZooFunction, class Replier, class CCallback, class... Args>
    auto zoo_watched_command(ZooFunction f, const path_t& path, Replier&& replier, CCallback cb,
                             replier_ptr<watch_reply_t> watcher, Args&&... args) -> void;

    auto create_prefix() -> void;

    auto format_path(const path_t& path) -> path_t;

    auto reconnect(zhandle_ptr& old_handle) -> void;

    auto init() -> zhandle_ptr;

    auto close(zhandle_t* handle) -> void;

    auto check_connectivity() -> void;

    auto cancel_watches() -> void;

    static auto check_rc(int rc) -> void;

    cfg_t cfg;
    session_t session;

    // executor for closing connections to avoid deadlocks
    std::unique_ptr<cocaine::api::executor_t> executor;

    cocaine::synchronized<zhandle_ptr> zhandle;

    size_t id_counter;
    cocaine::synchronized<watchers_t> watchers;
};

} // namespace zookeeper
} // namespace cocaine

