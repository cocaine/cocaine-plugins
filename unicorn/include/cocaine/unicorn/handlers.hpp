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

#include "cocaine/unicorn.hpp"

namespace cocaine {
namespace service {

/**
* Action for handling get requests during subscription.
* When client subscribes for a path - we make a get request to ZK with setting watch on specified path.
* On each get completion we compare last sent verison to client with current and if current version is greater send update to client.
* On each watch invoke we issue get command (to later process with this handler) with new watcher.
*/
struct unicorn_dispatch_t::subscribe_action_t :
    public zookeeper::managed_data_handler_base_t,
    public zookeeper::managed_watch_handler_base_t,
    public zookeeper::managed_stat_handler_base_t
{

    subscribe_action_t(
        const zookeeper::handler_tag& tag,
        unicorn_dispatch_t::response::subscribe _result,
        unicorn_service_t* _service,
        unicorn::path_t _path
    );

    /**
    * Handling get events
    */
    virtual
    void
    operator()(int rc, std::string value, const zookeeper::node_stat& stat);

    /**
    * Handling exist events in case node does not exist.
    */
    virtual
    void
    operator()(int rc, zookeeper::node_stat const& stat);

    /**
    * Handling watch events
    */
    virtual
    void
    operator()(int type, int state, zookeeper::path_t path);

    unicorn_dispatch_t::response::subscribe result;
    unicorn_service_t* service;
    std::mutex write_lock;
    unicorn::version_t last_version;
    const unicorn::path_t path;
};

/**
* Action for handling requests during subscription for childs.
* When client subscribes for a path - we make a get request to ZK with setting watch on specified path.
* On each get completion we compare last sent verison to client with current and if current version is greater send update to client.
* On each watch invoke we issue child command (to later process with this handler) starting new watch.
*/
struct unicorn_dispatch_t::children_subscribe_action_t :
    public zookeeper::managed_strings_stat_handler_base_t,
    public zookeeper::managed_watch_handler_base_t
{

    children_subscribe_action_t(
        const zookeeper::handler_tag& tag,
        unicorn_dispatch_t::response::children_subscribe,
        unicorn_service_t* _service,
        unicorn::path_t _path
    );

    /**
    * Handling child requests
    */
    virtual void
    operator()(int rc, std::vector<std::string> childs, const zookeeper::node_stat& stat);

    /**
    * Handling watch
    */
    virtual void
    operator()(int type, int state, zookeeper::path_t path);


    unicorn_dispatch_t::response::children_subscribe result;
    unicorn_service_t* service;
    std::mutex write_lock;
    unicorn::version_t last_version;
    const unicorn::path_t path;
};


struct unicorn_dispatch_t::put_action_t :
    public zookeeper::managed_stat_handler_base_t,
    public zookeeper::managed_data_handler_base_t
{

    put_action_t(
        const zookeeper::handler_tag& tag,
        unicorn_service_t* _service,
        unicorn_dispatch_t::response::put result,
        unicorn::path_t _path,
        unicorn::value_t _value,
        unicorn::version_t _version
    );

    /**
    * handling set request
    */
    virtual void
    operator()(int rc, zookeeper::node_stat const& stat);

    /**
    * handling get request after version mismatch
    */
    virtual void
    operator()(int rc, zookeeper::value_t value, zookeeper::node_stat const& stat);

    unicorn_service_t* service;
    unicorn_dispatch_t::response::put result;
    unicorn::path_t path;
    unicorn::value_t initial_value;
    zookeeper::value_t encoded_value;
    unicorn::version_t version;
};


/**
* Base handler for node creation. Used in create, lock, increment requests
*/
struct unicorn_dispatch_t::create_action_base_t :
    public zookeeper::managed_string_handler_base_t
{

    create_action_base_t(
        const zookeeper::handler_tag& tag,
        unicorn_service_t* _service,
        unicorn::path_t _path,
        unicorn::value_t _value,
        bool _ephemeral,
        bool _sequence
    );

    /**
    * called from create subrequest.
    */
    virtual void
    operator()(int rc, zookeeper::value_t value);

    /**
    * Called on success
    */
    virtual void
    finalize(zookeeper::value_t) = 0;

    /**
    * Called on failure
    */
    virtual void
    abort(int rc) = 0;

    int depth;
    unicorn_service_t* service;
    unicorn::path_t path;
    unicorn::value_t initial_value;
    zookeeper::value_t encoded_value;
    bool ephemeral;
    bool sequence;
};

/**
* Handler for simple node creation
*/
struct unicorn_dispatch_t::create_action_t:
    public create_action_base_t
{
    create_action_t(
        const zookeeper::handler_tag& tag,
        unicorn_service_t* _service,
        unicorn_dispatch_t::response::create result,
        unicorn::path_t _path,
        unicorn::value_t _value
    );

    virtual void
    finalize(zookeeper::value_t);

    virtual void
    abort(int rc);

    unicorn_dispatch_t::response::create result;
};

/**
* Handler for delete request to ZK.
*/
struct unicorn_dispatch_t::del_action_t :
    public zookeeper::void_handler_base_t
{
    del_action_t(unicorn_dispatch_t::response::del _result);

    virtual void
    operator()(int rc);

    unicorn_dispatch_t::response::del result;
};

/**
* Context for handling increment queries to service.
*/
struct unicorn_dispatch_t::increment_action_t:
    public create_action_base_t,
    public zookeeper::managed_stat_handler_base_t,
    public zookeeper::managed_data_handler_base_t
{

    increment_action_t(
        const zookeeper::handler_tag& tag,
        unicorn_service_t* _service,
        unicorn_dispatch_t::response::increment _result,
        unicorn::path_t _path,
        unicorn::value_t _increment,
        const std::shared_ptr<zookeeper::handler_scope_t>& _scope
    );

    /**
    * Get part of increment
    */
    virtual void
    operator()(int rc, const zookeeper::node_stat& stat);

    /**
    * Put part of increment
    */
    virtual void
    operator() (int rc, zookeeper::value_t value, const zookeeper::node_stat& stat);

    /**
    * Create part of increment
    */
    virtual void
    finalize(zookeeper::value_t);

    virtual void
    abort(int rc);

    unicorn_service_t* service;
    unicorn_dispatch_t::response::increment result;
    unicorn::value_t total;
    std::weak_ptr<zookeeper::handler_scope_t> scope;
};

/**
* Lock mechanism is described here:
* http://zookeeper.apache.org/doc/r3.1.2/recipes.html#sc_recipes_Locks
*/
struct distributed_lock_t::lock_action_t :
    public unicorn_dispatch_t::create_action_base_t,
    public zookeeper::managed_strings_stat_handler_base_t,
    public zookeeper::managed_stat_handler_base_t,
    public zookeeper::managed_watch_handler_base_t

{
    lock_action_t(
        const zookeeper::handler_tag& tag,
        unicorn_service_t* service,
        std::shared_ptr<distributed_lock_t::lock_state_t> state,
        unicorn::path_t _path,
        unicorn::path_t folder,
        unicorn::value_t _value,
        unicorn_dispatch_t::response::lock _result
    );

    /**
    * Childs subrequest handler
    */
    virtual void
    operator()(int rc, std::vector<std::string> childs, zookeeper::node_stat const& stat);

    /**
    * Exists subrequest handler
    */
    virtual void
    operator()(int rc, zookeeper::node_stat const& stat);


    /**
    * Watcher handler to watch on lock release.
    */
    virtual void operator()(int type, int state, unicorn::path_t path);

    /**
    * Lock creation handler.
    */
    virtual void
    finalize(zookeeper::value_t);

    virtual void
    abort(int rc);

    std::shared_ptr<distributed_lock_t::lock_state_t> state;
    unicorn_dispatch_t::response::lock result;
    unicorn::path_t folder;
    std::string created_node_name;
};

/**
* Handler for lock_release.
*/
struct distributed_lock_t::release_lock_action_t :
    public zookeeper::void_handler_base_t
{
    release_lock_action_t(unicorn_service_t* _service);

    virtual void
        operator()(int rc);

    unicorn_service_t* service;
};



}}
