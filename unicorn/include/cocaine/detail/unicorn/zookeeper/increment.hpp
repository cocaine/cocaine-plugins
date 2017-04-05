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

#include "cocaine/detail/unicorn/zookeeper/create.hpp"

#include "cocaine/detail/zookeeper/handler.hpp"

namespace cocaine { namespace unicorn {

/**
* Context for handling increment queries to service.
*/
struct increment_action_t :
    public create_action_base_t,
    public zookeeper::managed_stat_handler_base_t,
    public zookeeper::managed_data_handler_base_t
{

    increment_action_t(const zookeeper::handler_tag& tag,
                       zookeeper_t::context_t ctx,
                       api::unicorn_t::callback::increment callback,
                       path_t _path,
                       value_t _increment);

    /**
    * Get part of increment
    */
    virtual void
    stat_event(int rc, const zookeeper::node_stat& stat);

    /**
    * Put part of increment
    */
    virtual void
    data_event(int rc, zookeeper::value_t value, const zookeeper::node_stat& stat);

    /**
    * Implicit call to base.
    */
    virtual void
    string_event(int rc, zookeeper::value_t value) {
        return create_action_base_t::string_event(rc, std::move(value));
    }

    /**
    * Create part of increment
    */
    virtual void
    finalize(zookeeper::value_t);

    virtual void
    abort(int rc);

    api::unicorn_t::callback::increment callback;
    value_t total;
};

}} //namespace cocaine::unicorn
