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
* Base handler for node creation. Used in create, lock, increment requests
*/
struct create_action_base_t :
    public zookeeper::managed_string_handler_base_t
{
    create_action_base_t(const zookeeper::handler_tag& tag,
                         zookeeper_t::context_t ctx,
                         path_t _path,
                         value_t _value,
                         bool _ephemeral,
                         bool _sequence);

    /**
    * called from create subrequest.
    */
    virtual void
    string_event(int rc, zookeeper::path_t value);

    /**
    * Called on success
    */
    virtual void
    finalize(zookeeper::path_t) = 0;

    /**
    * Called on failure
    */
    virtual void
    abort(int rc) = 0;

    int depth;
    zookeeper_t::context_t ctx;
    path_t path;
    value_t initial_value;
    zookeeper::value_t encoded_value;
    bool ephemeral;
    bool sequence;
};


/**
* Handler for simple node creation
*/
struct create_action_t:
    public create_action_base_t
{
    create_action_t(const zookeeper::handler_tag& tag,
                    zookeeper_t::context_t ctx,
                    api::unicorn_t::callback::create callback,
                    path_t _path,
                    value_t _value,
                    bool ephemeral,
                    bool sequence);

    virtual void
    finalize(zookeeper::value_t);

    virtual void
    abort(int rc);

    api::unicorn_t::callback::create callback;
};

}} //namespace cocaine::unicorn
