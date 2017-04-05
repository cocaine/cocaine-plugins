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

#include "cocaine/api/unicorn.hpp"

#include "cocaine/detail/unicorn/zookeeper.hpp"

#include "cocaine/detail/zookeeper/handler.hpp"

namespace cocaine { namespace unicorn {

/**
* Action for handling get requests during subscription.
* When client subscribes for a path - we make a get request to ZK with setting watch on specified path.
* On each get completion we compare last sent verison to client with current and if current version is greater send update to client.
* On each watch invoke we issue get command (to later process with this handler) with new watcher.
*/
struct subscribe_action_t :
    public zookeeper::managed_data_handler_base_t,
    public zookeeper::managed_watch_handler_base_t,
    public zookeeper::managed_stat_handler_base_t
{
    subscribe_action_t(const zookeeper::handler_tag& tag,
                       api::unicorn_t::callback::subscribe callback,
                       unicorn::zookeeper_t::context_t ctx,
                       unicorn::path_t path);

    /**
    * Handling get events
    */
    virtual
    void
    data_event(int rc, std::string value, const zookeeper::node_stat& stat);

    /**
    * Handling exist events in case node does not exist.
    */
    virtual
    void
    stat_event(int rc, zookeeper::node_stat const& stat);

    /**
    * Handling watch events
    */
    virtual
    void
    watch_event(int type, int state, zookeeper::path_t path);

    api::unicorn_t::callback::subscribe callback;
    unicorn::zookeeper_t::context_t ctx;
    std::mutex write_lock;
    unicorn::version_t last_version;
    const unicorn::path_t path;
};

}} //namespace cocaine::unicorn
