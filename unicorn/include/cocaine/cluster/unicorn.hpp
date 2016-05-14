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

#include <cocaine/api/cluster.hpp>
#include <cocaine/api/unicorn.hpp>
#include <cocaine/forwards.hpp>
#include <cocaine/locked_ptr.hpp>

#include <set>
namespace cocaine { namespace cluster {

class unicorn_cluster_t:
    public api::cluster_t
{
public:

    struct cfg_t {
        cfg_t(const dynamic_t& args);

        unicorn::path_t path;
        size_t retry_interval;
        size_t check_interval;
    };

    unicorn_cluster_t(context_t& context, interface& locator, const std::string& name, const dynamic_t& args);

private:
    struct on_announce;
    struct on_update;
    struct on_fetch;
    struct on_list_update;

    void
    announce();

    void
    subscribe();

    void
    on_announce_timer(const std::error_code& ec);

    void
    on_subscribe_timer(const std::error_code& ec);

    void
    on_node_list_change(std::future<api::unicorn_t::response::children_subscribe> new_list);

    void
    on_node_fetch(const std::string& uuid, std::future<api::unicorn_t::response::get> node_endpoints);

    void
    on_announce_set(std::future<api::unicorn_t::response::create> future);

    void
    on_announce_checked(std::future<api::unicorn_t::response::subscribe> future);

    std::shared_ptr<logging::logger_t> log;
    cfg_t config;
    cocaine::context_t& context;
    cluster_t::interface& locator;
    std::vector<asio::ip::tcp::endpoint> endpoints;
    asio::deadline_timer announce_timer;
    asio::deadline_timer subscribe_timer;
    api::unicorn_ptr unicorn;
    api::unicorn_scope_ptr create_scope, subscribe_scope, children_subscribe_scope, get_scope;
    typedef std::map<std::string, std::vector<asio::ip::tcp::endpoint>> locator_endpoints_t;
    synchronized<locator_endpoints_t> registered_locators;
};

}}
