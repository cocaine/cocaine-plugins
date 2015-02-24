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

typedef std::shared_ptr<unicorn_t::subscribe_context_t> subscribe_context_ptr;
typedef std::shared_ptr<unicorn_t::lsubscribe_context_t> lsubscribe_context_ptr;
typedef std::unique_ptr<unicorn_t::put_context_base_t> put_context_ptr;
typedef std::unique_ptr<unicorn_t::increment_context_t> increment_context_ptr;

/**
* Handler used to create nodes during some operation if it is not present.
* It is called recursively until it can create a node or fail with error different from ZNONODE
*/
struct unicorn_t::nonode_action_t :
    public zookeeper::string_handler_base_t
{
    nonode_action_t(put_context_ptr _context, int _depth = 0);

    virtual void
    operator()(int rc, std::string value);

    put_context_ptr context;
    int depth;
};

/**
* Base class of context of subscribe operation.
* It persists until subscription is active is being moved from one watch handler to another.
*/
struct unicorn_t::subscribe_context_base_t :
    public std::enable_shared_from_this<unicorn_t::subscribe_context_base_t>
{
    subscribe_context_base_t(unicorn_t* _parent, path_t _path);

    void
    abort(int rc, const std::string& message);

    virtual void
    on_abort(int rc, const std::string& message) = 0;

    bool
    aborted();

    unicorn_t* parent;
    std::mutex write_lock;
    version_t last_version;
    const path_t path;

private:
    bool is_aborted;
};

/**
* Context of subscribe operation for node.
* It persists until subscription is active is being moved from one watch handler to another.
*/
struct unicorn_t::subscribe_context_t :
    public unicorn_t::subscribe_context_base_t
{
    subscribe_context_t(unicorn_t* _parent, unicorn_t::response::subscribe _result, path_t _path);

    ~subscribe_context_t();

    virtual void
    on_abort(int rc, const std::string& message);

    unicorn_t::response::subscribe result;
};

/**
* Action for handling get requests during subscription.
* When client subscribes for a path - we make a get request to ZK with setting watch on specified path.
* On each get completion we compare last sent verison to client with current and if current version is greater send update to client.
* On each watch invoke we issue get command (to later process with this handler) with new watcher.
*/
struct unicorn_t::subscribe_action_t :
    public zookeeper::data_handler_with_watch_t
{

    subscribe_action_t(subscribe_context_ptr _context);

    void operator()(int rc, std::string value, const zookeeper::node_stat& stat);

    subscribe_context_ptr context;
};

struct unicorn_t::subscribe_nonode_action_t :
    public zookeeper::stat_handler_with_watch_t
{
    subscribe_nonode_action_t(subscribe_context_ptr _context);

    virtual void
    operator()(int rc, zookeeper::node_stat const& stat);

    subscribe_context_ptr context;
};

/**
* See subscribe_action_t
* Callback to be called when ZK send watch event on a node indicating it has changed.
* It issues new get command to ZK with new watch.
*/
struct unicorn_t::subscribe_watch_handler_t :
    public zookeeper::watch_handler_base_t
{
    subscribe_watch_handler_t(subscribe_context_ptr _context);


    virtual void
    operator()(int type, int state, zookeeper::path_t path);

    subscribe_context_ptr context;
};

/**
* Context of lsubscribe operation.
* It persists until subscription for childs is active and is being moved from one watch handler to another.
*/
struct unicorn_t::lsubscribe_context_t :
    public unicorn_t::subscribe_context_base_t
{
    lsubscribe_context_t(unicorn_t* _parent, unicorn_t::response::lsubscribe _result, path_t _path);

    virtual void
    on_abort(int rc, const std::string& message);

    unicorn_t::response::lsubscribe result;
};

/**
* Action for handling get requests during subscription for childs.
* When client subscribes for a path - we make a get request to ZK with setting watch on specified path.
* On each get completion we compare last sent verison to client with current and if current version is greater send update to client.
* On each watch invoke we issue child command (to later process with this handler) with new watcher.
*/
struct unicorn_t::lsubscribe_action_t :
    public zookeeper::strings_stat_handler_with_watch_t
{

    lsubscribe_action_t(lsubscribe_context_ptr _context);

    virtual void
    operator()(int rc, std::vector<std::string> childs, const zookeeper::node_stat& stat);

    lsubscribe_context_ptr context;
};


/**
* See lsubscribe_action_t
* Callback to be called when ZK send watch event on a node indicating it has changed.
* It issues new get command to ZK with new watch.
*/
struct unicorn_t::lsubscribe_watch_handler_t :
    public zookeeper::watch_handler_base_t
{
    lsubscribe_watch_handler_t(lsubscribe_context_ptr _context);

    virtual void
    operator()(int type, int state, zookeeper::path_t path);

    lsubscribe_context_ptr context;
};

struct unicorn_t::put_context_base_t {

    put_context_base_t(unicorn_t* _parent, path_t _path, value_t _value, version_t _version, bool _ephemeral);

    virtual
    ~put_context_base_t() {}

    virtual void
    finalize(versioned_value_t) = 0;

    virtual void
    abort(int rc, const std::string& message) = 0;

    unicorn_t* parent;
    path_t path;
    value_t initial_value;
    zookeeper::value_t value;
    version_t version;
    bool ephemeral;
};

/**
* Context of put operation.
* It persists until result of operation is determined and is being moved between different ZK handlers
*/
struct unicorn_t::put_context_t :
    public unicorn_t::put_context_base_t
{
    put_context_t(unicorn_t* _parent, path_t _path, value_t _value, version_t _version, unicorn_t::response::put _result);

    ~put_context_t();

    virtual void
    finalize(versioned_value_t cur_value);

    virtual void
    abort(int rc, const std::string& message);

    unicorn_t::response::put result;
};

/**
* Context of create operation.
* It persists until result of operation is determined and is being moved between different ZK handlers
*/
struct unicorn_t::create_context_t :
    public unicorn_t::put_context_base_t
{
    create_context_t(unicorn_t* _parent, path_t _path, value_t _value, unicorn_t::response::create _result);

    virtual void
        finalize(versioned_value_t cur_value);

    virtual void
        abort(int rc, const std::string& message);

    unicorn_t::response::create result;
};


/**
* Handler for put request to ZK.
* If put succeded - writes value to result
* If there is a recoverable failure (ZNONODE) - tries to create a node with specified value (recursively, see put_nonode_action_t)
* If there is a version mismatch - makes a request to get new version to send back to client
* Writes error to result on other erros.
*/
struct unicorn_t::put_action_t :
    public zookeeper::stat_handler_base_t
{
    put_action_t(put_context_ptr _context);

    virtual void
    operator()(int rc, zookeeper::node_stat const& stat);

    put_context_ptr context;
};

/**
* Handler for GET request to ZK after failed put because of version mismatch.
* Writes curent value in ZK to client, or error in case of any present error.
*/
struct unicorn_t::put_badversion_action_t :
    public zookeeper::data_handler_base_t
{
    put_badversion_action_t(put_context_ptr _context);

    virtual void
    operator()(int rc, std::string value, zookeeper::node_stat const& stat);

    put_context_ptr context;
};

/**
* Handler for delete request to ZK.
*/
struct unicorn_t::del_action_t :
    public zookeeper::void_handler_base_t
{
    del_action_t(unicorn_t::response::del _result);

    virtual void
    operator()(int rc);

    unicorn_t::response::del result;
};

/**
* Context for handling increment queries to service.
* This is needed because increment is emulated via GET-PUT command.
* It persists until increment is completed, or unrecoverable error occured
*/
struct unicorn_t::increment_context_t {

    increment_context_t(unicorn_t* _parent, unicorn_t::response::increment _result, path_t _path, value_t _increment);
    ~increment_context_t();

    unicorn_t* parent;
    unicorn_t::response::increment result;
    const path_t path;
    value_t increment;
    value_t total;
};

/**
* Handler for set subrequest during increment command.
* If version has changed after GET subcommand calls GET again
*/
struct unicorn_t::increment_set_action_t :
    public zookeeper::stat_handler_base_t
{
    increment_set_action_t(increment_context_ptr _context);

    virtual void
    operator()(int rc, zookeeper::node_stat const& stat);

    increment_context_ptr context;
};

/**
* Handler for get subrequest during increment command.
*/
struct unicorn_t::increment_get_action_t :
    public zookeeper::data_handler_base_t
{
    increment_get_action_t(increment_context_ptr _context);

    virtual void
    operator()(int rc, zookeeper::value_t value, zookeeper::node_stat const& stat);

    increment_context_ptr context;
};

struct distributed_lock_t::put_ephemeral_context_t :
    public unicorn_t::put_context_base_t
{
    put_ephemeral_context_t(
        unicorn_t* unicorn,
        distributed_lock_t* _parent,
        path_t _path, value_t _value,
        version_t _version,
        unicorn_t::response::acquire _result);

    ~put_ephemeral_context_t();

    virtual void
    finalize(versioned_value_t cur_value);

    virtual void
    abort(int rc, const std::string& message);

    unicorn_t::response::acquire result;
    distributed_lock_t* cur_parent;
};

}}
