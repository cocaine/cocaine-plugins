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

#include <boost/optional/optional.hpp>

#include <set>

namespace cocaine { namespace cluster {

class unicorn_cluster_t:
    public api::cluster_t
{
public:
    typedef api::unicorn_t::response response;

    struct cfg_t {
        cfg_t(const dynamic_t& args);

        unicorn::path_t path;
        size_t retry_interval;
    };

    class timer_t {
        unicorn_cluster_t& parent;
        synchronized<asio::deadline_timer> timer;
        std::function<void()> callback;

    public:
        timer_t(unicorn_cluster_t& parent, std::function<void()> callback);
        auto defer_retry() -> void;

    private:
        auto defer(const boost::posix_time::time_duration& duration) -> void;
    };

    class announcer_t {
        unicorn_cluster_t& parent;

        timer_t timer;
        std::string path;
        std::vector<asio::ip::tcp::endpoint> endpoints;

        api::auto_scope_t scope;

    public:
        announcer_t(unicorn_cluster_t& parent);

        auto announce() -> void;

    private:
        auto on_set(std::future<response::create> future) -> void;

        auto on_check(std::future<response::subscribe> future) -> void;

        auto set_and_check_endpoints() -> bool;

        auto defer_announce_retry() -> void;
    };

    class subscriber_t {
        struct locator_subscription_t {
            std::vector<asio::ip::tcp::endpoint> endpoints;
            api::auto_scope_t scope;
        };

        using subscriptions_t = std::map<std::string, locator_subscription_t>;

        synchronized<subscriptions_t> subscriptions;
        unicorn_cluster_t& parent;
        timer_t timer;
        api::auto_scope_t children_scope;

    public:
        subscriber_t(unicorn_cluster_t& parent);
        auto subscribe() -> void;

    private:
        auto on_children(std::future<response::children_subscribe> future) -> void;
        auto on_node(std::string uuid, std::future<response::subscribe> future) -> void;
        auto update_state(std::vector<std::string> nodes) -> void;
    };


    unicorn_cluster_t(context_t& context, interface& locator, mode_t mode, const std::string& name, const dynamic_t& args);

private:
    std::shared_ptr<logging::logger_t> log;
    cfg_t config;
    cocaine::context_t& context;
    cluster_t::interface& locator;
    api::unicorn_ptr unicorn;
    announcer_t announcer;
    subscriber_t subscriber;
};

}}
