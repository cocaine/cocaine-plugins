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

#ifndef COCAINE_UNICORN_CLUSTER_HPP
#define COCAINE_UNICORN_CLUSTER_HPP

#include "cocaine/api/cluster.hpp"
#include "cocaine/unicorn.hpp"

#include <cocaine/context.hpp>

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

    struct on_announce:
        public unicorn::writable_adapter_base_t<unicorn::api_t::response::create_result>,
        public std::enable_shared_from_this<on_announce>
    {
        on_announce(unicorn_cluster_t* _parent);

        virtual void
        write(unicorn::api_t::response::create_result&& result);

        using unicorn::writable_adapter_base_t<unicorn::api_t::response::create_result>::abort;

        virtual void
        abort(const std::error_code& ec);

        unicorn_cluster_t* parent;
    };

    struct on_update:
        public unicorn::writable_adapter_base_t<unicorn::api_t::response::subscribe_result>,
        public std::enable_shared_from_this<on_update>
    {
        on_update(unicorn_cluster_t* _parent);

        virtual void
        write(unicorn::api_t::response::subscribe_result&& result);

        using unicorn::writable_adapter_base_t<unicorn::api_t::response::subscribe_result>::abort;

        virtual void
        abort(const std::error_code& ec);

        unicorn_cluster_t* parent;
    };

    struct on_fetch :
        public unicorn::writable_adapter_base_t<unicorn::api_t::response::get_result>
    {
        on_fetch(std::string uuid, unicorn_cluster_t* _parent);

        virtual void
        write(unicorn::api_t::response::get_result&& result);

        using unicorn::writable_adapter_base_t<unicorn::api_t::response::get_result>::abort;

        virtual void
        abort(const std::error_code& ec);

        std::string uuid;
        unicorn_cluster_t* parent;
    };

    struct on_list_update :
        public unicorn::writable_adapter_base_t<unicorn::api_t::response::children_subscribe_result>
    {
        on_list_update(unicorn_cluster_t* _parent);

        virtual void
        write(unicorn::api_t::response::children_subscribe_result&& result);

        using unicorn::writable_adapter_base_t<unicorn::api_t::response::children_subscribe_result>::abort;

        virtual void
        abort(const std::error_code& ec);

        unicorn_cluster_t* parent;
    };

private:
    void
    announce();

    void
    subscribe();

    void
    on_announce_timer(const std::error_code& ec);

    void
    on_subscribe_timer(const std::error_code& ec);

    std::shared_ptr<cocaine::logging::log_t> log;
    cfg_t config;
    cocaine::context_t& context;
    cluster_t::interface& locator;
    std::vector<asio::ip::tcp::endpoint> endpoints;
    asio::deadline_timer announce_timer;
    asio::deadline_timer subscribe_timer;
    zookeeper::session_t zk_session;
    zookeeper::connection_t zk;
    unicorn::zookeeper_api_t unicorn;
    synchronized<std::set<std::string>> registered_locators;
};

}}

#endif
