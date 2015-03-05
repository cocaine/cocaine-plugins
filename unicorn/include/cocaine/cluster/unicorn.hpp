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
#include <cocaine/api/cluster.hpp>

namespace cocaine { namespace cluster {

class unicorn_cluster_t:
    public api::cluster_t
{
public:
    struct cfg_t {
        cfg_t(const dynamic_t& args);

        unicorn::path_t path;
    };
    unicorn_cluster_t(context_t& context, interface& locator, const std::string& name, const dynamic_t& args);

    struct on_announce:
        public writable_adapter_base_t<service::unicorn_dispatch_t::response::create_result>,
        public std::enable_shared_from_this<on_announce>
    {
        on_announce(unicorn_cluster_t* _parent);

        virtual void
        write(service::unicorn_dispatch_t::response::create_result&& result);

        virtual void
        abort(int rc, const std::string& reason);

        unicorn_cluster_t* parent;
    };

    struct on_update:
        public writable_adapter_base_t<service::unicorn_dispatch_t::response::subscribe_result>,
        public std::enable_shared_from_this<on_update>
    {
        on_update(unicorn_cluster_t* _parent);

        virtual void
        write(service::unicorn_dispatch_t::response::subscribe_result&& result);

        virtual void
        abort(int rc, const std::string& reason);

        unicorn_cluster_t* parent;
    };

    struct on_fetch :
        public writable_adapter_base_t<service::unicorn_dispatch_t::response::get_result>
    {
        on_fetch(std::string uuid, unicorn_cluster_t* _parent);

        virtual void
        write(service::unicorn_dispatch_t::response::get_result&& result);

        virtual void
        abort(int ec, const std::string& reason);

        std::string uuid;
        unicorn_cluster_t* parent;
    };

    struct on_list_update :
    public writable_adapter_base_t<service::unicorn_dispatch_t::response::lsubscribe_result>
    {
        on_list_update(unicorn_cluster_t* _parent);

        virtual void
        write(service::unicorn_dispatch_t::response::lsubscribe_result&& result);

        virtual void
        abort(int ec, const std::string& reason);

        unicorn_cluster_t* parent;
    };
private:

    void
    init();

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
    asio::deadline_timer announce_timer;
    asio::deadline_timer subscribe_timer;
    std::shared_ptr<cocaine::service::unicorn_dispatch_t> unicorn;
    synchronized<std::set<std::string>> registered_locators;
    static constexpr size_t retry_interval = 1;
};

}}

#endif