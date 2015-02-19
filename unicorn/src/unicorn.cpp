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
#include "cocaine/unicorn/value.hpp"

#include "cocaine/zookeeper/handler.hpp"

#include <cocaine/context.hpp>
#include <asio/io_service.hpp>

using namespace cocaine::unicorn;

namespace cocaine {
namespace service {

namespace {
/**
* Converts dynamic_t to zookepers config.
*/
zookeeper::cfg_t make_zk_config(const dynamic_t& args) {
    const auto& cfg = args.as_object();
    const auto& endpoints_cfg = cfg.at("endpoints", dynamic_t::empty_array).as_array();
    std::vector<zookeeper::cfg_t::endpoint_t> endpoints;
    for (size_t i = 0; i < endpoints_cfg.size(); i++) {
        endpoints.emplace_back(endpoints_cfg[i].as_object().at("host").as_string(), endpoints_cfg[i].as_object().at("port").as_uint());
    }
    if (endpoints.empty()) {
        endpoints.emplace_back("localhost", 2181);
    }
    return zookeeper::cfg_t(endpoints, cfg.at("recv_timeout", 1000u).as_uint());
}
}

typedef std::shared_ptr<unicorn_t::subscribe_context_t> subscribe_context_ptr;
typedef std::unique_ptr<unicorn_t::put_context_t> put_context_ptr;
typedef std::unique_ptr<unicorn_t::increment_context_t> increment_context_ptr;

/**
* Context of put operation.
* It persists until result of operation is determined and is being moved between different ZK handlers
*/
struct unicorn_t::put_context_t {
    unicorn_t* parent;
    path_t path;
    value_t initial_value;
    zookeeper::value_t value;
    version_t version;
    unicorn_t::response::put result;

    put_context_t(unicorn_t* _parent, path_t _path, value_t _value, version_t _version, unicorn_t::response::put _result) :
        parent(_parent),
        path(std::move(_path)),
        initial_value(std::move(_value)),
        value(serialize(initial_value)),
        version(std::move(_version)),
        result(std::move(_result)) {
    }

};

/**
* Context of subscribe operation.
* It persists until subscription is active is being moved from one watch handler to another.
*/
struct unicorn_t::subscribe_context_t :
    public std::enable_shared_from_this<unicorn_t::subscribe_context_t>
{
    unicorn_t* parent;
    unicorn_t::response::subscribe result;
    std::mutex write_lock;
    version_t last_version;
    const path_t path;

    subscribe_context_t(unicorn_t* _parent, unicorn_t::response::subscribe _result, path_t _path, version_t version) :
        parent(_parent),
        result(std::move(_result)),
        write_lock(),
        last_version(std::move(version)),
        path(std::move(_path)),
        is_aborted(false) {
    }

    inline void abort(int rc, const std::string& message) {
        is_aborted = true;
        result.abort(rc, message);
    }

    inline bool aborted() {
        return is_aborted;
    }

private:
    bool is_aborted;
};

/**
* Handler used to create nodes during some operation if it is not present.
* It is called recursively until it can create a node or fail with error different from ZNONODE
* After suceess calls finalize()
* After failure calls abort()
*/
struct unicorn_t::nonode_action_t :
    public zookeeper::string_handler_base_t
{
    nonode_action_t(put_context_ptr _context, int _depth = 0) :
        context(std::move(_context)),
        depth(_depth) {
    }

    virtual void operator()(int rc, std::string value) {
        if (rc == ZOK) {
            if (depth == 0) {
                context->result.write(versioned_value_t(std::move(context->initial_value), version_t()));
            }
            else {
                auto& context_ref = *context;
                auto ptr = std::make_unique<nonode_action_t>(std::move(context), depth - 1);
                if (depth == 1) {
                    context_ref.parent->zk.create(context_ref.path, context_ref.value, std::move(ptr));
                }
                else {
                    context_ref.parent->zk.create(context_ref.path, "", std::move(ptr));
                }
            }
        }
        else if (rc == ZNONODE) {
            auto& context_ref = *context;
            auto ptr = std::make_unique<nonode_action_t>(std::move(context), depth + 1);
            context_ref.parent->zk.create(zookeeper::path_parent(context_ref.path, depth), "", std::move(ptr));
        }
        else {
            context->result.abort(rc, "Error during node creation subrequest: " + zookeeper::get_error_message(rc));
        }
    }

    put_context_ptr context;
    int depth;
};

/**
* Handler for GET request to ZK after failed put because of version mismatch.
* Writes curent value in ZK to client, or error in case of any present error.
*/
struct unicorn_t::put_badversion_action_t :
    public zookeeper::data_handler_base_t {
    put_badversion_action_t(put_context_ptr _context) :
        context(std::move(_context)) {
    }

    virtual void operator()(int rc, std::string value, zookeeper::node_stat const& stat) {
        if (rc) {
            context->result.abort(rc, "Failed get after version mismatch:" + zookeeper::get_error_message(rc));
        }
        else {
            try {
                context->result.write(versioned_value_t(unserialize(value), stat.version));
            }
            catch (const std::exception& e) {
                rc = zookeeper::ZOO_EXTRA_ERROR::INVALID_VALUE;
                context->result.abort(rc, "Error during new value get:" + zookeeper::get_error_message(rc, e));
            }
        }
    }

    put_context_ptr context;
};

/**
* Handler for put request to ZK.
* If put succeded - writes value to result
* If there is a recoverable failure (ZNONODE) - tries to create a node with specified value (recursively, see put_nonode_action_t)
* If there is a version mismatch - makes a request to get new version to send back to client
* Writes error to result on other erros.
*/
struct unicorn_t::put_action_t :
    public zookeeper::stat_handler_base_t {
    put_action_t(put_context_ptr _context) :
        context(std::move(_context)) {
    }

    virtual void operator()(int rc, zookeeper::node_stat const& stat) {
        if (rc == ZNONODE && context->version == -1) {
            auto& context_ref = *context;
            auto ptr = std::make_unique<nonode_action_t>(std::move(context));
            context_ref.parent->zk.create(context_ref.path, context_ref.value, std::move(ptr));
        }
        else if(rc == ZNONODE) {
            context->result.write(versioned_value_t(value_t(), -1));
        }
        else if (rc == ZBADVERSION) {
            auto& context_ref = *context;
            auto ptr = std::make_unique<put_badversion_action_t>(std::move(context));
            context_ref.parent->zk.get(context_ref.path, std::move(ptr));
        }
        else if (rc != 0) {
            context->result.abort(rc, zookeeper::get_error_message(rc));
        }
        else {
            context->result.write(versioned_value_t(context->initial_value, stat.version));
        }
    }

    put_context_ptr context;
};

/**
* Handler for delete request to ZK.
*/
struct unicorn_t::del_action_t :
    public zookeeper::void_handler_base_t {
    del_action_t(unicorn_t::response::del _result) :
        result(std::move(_result)) {
    }

    virtual void operator()(int rc) {
        if (rc) {
            result.abort(rc, zookeeper::get_error_message(rc));
        }
        else {
            result.write(true);
        }
    }

    unicorn_t::response::del result;
};

/**
* Context for handling increment queries to service.
* This is needed because increment is emulated via GET-PUT command.
* It persists until increment is completed, or unrecoverable error occured
*/
struct unicorn_t::increment_context_t {
    unicorn_t* parent;
    unicorn_t::response::increment result;
    const path_t path;
    value_t increment;
    value_t total;

    increment_context_t(unicorn_t* _parent, unicorn_t::response::increment _result, path_t _path, value_t _increment) :
        parent(_parent),
        result(std::move(_result)),
        path(std::move(_path)),
        increment(std::move(_increment)),
        total() {
    }
};

/**
* Handler for set subrequest during increment command.
* If version has changed after GET subcommand calls GET again
*/
struct unicorn_t::increment_set_action_t :
    public zookeeper::stat_handler_base_t
{
    increment_set_action_t(increment_context_ptr _context) :
        context(std::move(_context)) {
    }

    virtual void operator()(int rc, zookeeper::node_stat const& stat);

    increment_context_ptr context;
};

/**
* Handler for get subrequest during increment command.
*/
struct unicorn_t::increment_get_action_t :
    public zookeeper::data_handler_base_t {
    increment_get_action_t(increment_context_ptr _context) :
        context(std::move(_context)) {
    }

    virtual void operator()(int rc, zookeeper::value_t value, zookeeper::node_stat const& stat) {
        if(rc == ZNONODE) {
            /**
            * This is only permitted because put command and increment command have same return type.
            * If they would have different types we need an abstraction for completion of create command (bad)
            * OR separate chain for increment command in case of target node is not created (bad)
            * OR reject increments on unexisting nodes(bad)
            */
            auto context_ptr = std::make_unique<put_context_t>(
                context->parent,
                std::move(context->path),
                std::move(context->increment),
                version_t(),
                std::move(context->result)
            );
            auto& context_ref = *context_ptr;
            auto handler = std::make_unique<nonode_action_t>(std::move(context_ptr));
            context->parent->zk.create(context_ref.path, context_ref.value, std::move(handler));
        }
        else if (rc != ZOK) {
            context->result.abort(rc, "Error during value get:" + zookeeper::get_error_message(rc));
        }
        else {
            value_t parsed;
            if (!value.empty()) {
                try {
                    parsed = unserialize(value);
                }
                catch (const std::exception& e){
                    rc = zookeeper::ZOO_EXTRA_ERROR::INVALID_VALUE;
                    context->result.abort(rc, "Error during value get:" + zookeeper::get_error_message(rc, e));
                }
            }
            if (stat.numChildren != 0) {
                rc = zookeeper::ZOO_EXTRA_ERROR::CHILD_NOT_ALLOWED;
                context->result.abort(rc, "Error during value get:" + zookeeper::get_error_message(rc));
                return;
            }
            if (!parsed.is_double() && !parsed.is_int() && !parsed.is_null() && !parsed.is_uint()) {
                rc = zookeeper::ZOO_EXTRA_ERROR::INVALID_TYPE;
                context->result.abort(rc, "Error during value get:" + zookeeper::get_error_message(rc));
                return;
            }
            auto& context_ref = *context;
            auto handler = std::make_unique<increment_set_action_t>(std::move(context));
            if (parsed.is_double() || context_ref.increment.is_double()) {
                context_ref.total = parsed.to<double>() + context_ref.increment.to<double>();
                context_ref.parent->zk.put(context_ref.path, serialize(context_ref.total), stat.version, std::move(handler));
            }
            else {
                context_ref.total = parsed.to<int64_t>() + context_ref.increment.to<int64_t>();
                context_ref.parent->zk.put(context_ref.path, serialize(context_ref.total), stat.version, std::move(handler));
            }
        }
    }

    increment_context_ptr context;
};

void unicorn_t::increment_set_action_t::operator()(int rc, zookeeper::node_stat const& stat) {
    if (rc == ZOK) {
        context->result.write(versioned_value_t(context->total, stat.version));
    }
    else if (rc == ZBADVERSION) {
        const auto& context_ref = *context;
        auto increment_handler = std::make_unique<increment_get_action_t>(std::move(context));
        context_ref.parent->zk.get(context_ref.path, std::move(increment_handler));
    }
}

struct unicorn_t::subscribe_nonode_action_t :
    public zookeeper::stat_handler_with_watch_t
{
    subscribe_nonode_action_t(subscribe_context_ptr _context) :
        context(std::move(_context)) {
    }

    virtual void
    operator()(int rc, zookeeper::node_stat const& stat) {

    }

    subscribe_context_ptr context;
};

/**
* Action for handling get requests during subscription.
* When client subscribes for a path - we make a get request to ZK with setting watch on specified path.
* On each get completion we compare last sent verison to client with current and if current version is greater send update to client.
* On each watch invoke we issue get command (to later process with this handler) with new watcher.
*/
struct unicorn_t::subscribe_action_t :
    public zookeeper::data_handler_with_watch_t {

    subscribe_action_t(subscribe_context_ptr _context) :
        context(std::move(_context)) {
    }

    void operator()(int rc, std::string value, const zookeeper::node_stat& stat);

    subscribe_context_ptr context;
};

/**
* See subscribe_action_t
* Callback to be called when ZK send watch event on a node indicating it has changed.
* It issues new get command to ZK with new watch.
*/
struct unicorn_t::watch_handler_t :
    public zookeeper::watch_handler_base_t
{
    watch_handler_t(subscribe_context_ptr _context) :
        context(std::move(_context)) {
    }

    virtual void operator()(int type, int state, zookeeper::path_t path) {
        if (context->aborted()) {
            return;
        }
        auto& context_ref = *context;
        auto subscribe_handler = std::make_unique<subscribe_action_t>(context);
        auto watch_handler = std::make_unique<watch_handler_t>(std::move(context));
        context_ref.parent->zk.get(context_ref.path, std::move(subscribe_handler), std::move(watch_handler));
    }

    subscribe_context_ptr context;
};

void
unicorn_t::subscribe_action_t::operator()(int rc, std::string value, const zookeeper::node_stat& stat) {
    if(rc == ZNONODE) {
        if(context->last_version != -1) {
            context->abort(rc, zookeeper::get_error_message(rc));
        }
        else {
            context->result.write(versioned_value_t(value_t(), -1));
            auto& context_ref = *context;
            //TODO: Performance can be increased if we do not delete-create watcher object
            // but override run method and use old watcher.
            auto handler = std::make_unique<subscribe_nonode_action_t>(context);
            auto watcher = std::make_unique<watch_handler_t>(std::move(context));
            context_ref.parent->zk.exists(context_ref.path, std::move(handler), std::move(watcher));
        }
    }
    else if (rc != 0) {
        //TODO: handle nonexisting nodes.
        context->abort(rc, zookeeper::get_error_message(rc));
    }
    else if (stat.numChildren != 0) {
        rc = zookeeper::CHILD_NOT_ALLOWED;
        context->abort(rc, zookeeper::get_error_message(rc));
    }
    else {
        version_t new_version(stat.version);
        std::lock_guard<std::mutex> guard(context->write_lock);
        if (new_version > context->last_version) {
            context->last_version = new_version;
            value_t val;
            try {
                context->result.write(versioned_value_t(unserialize(value), new_version));
            }
            catch(const std::exception& e) {
                rc = zookeeper::ZOO_EXTRA_ERROR::INVALID_VALUE;
                context->result.abort(rc, zookeeper::get_error_message(rc) + ". Exception: " + e.what());
            }

        }
    }
}

unicorn_t::unicorn_t(context_t& context, asio::io_service& _asio, const std::string& name, const dynamic_t& args) :
    service_t(context, _asio, name, args),
    dispatch<io::unicorn_tag>(name),
    zk_session(),
    zk(make_zk_config(args), zk_session),
    log(context.log("unicorn")) {
    using namespace std::placeholders;
    on<io::unicorn::subscribe>(std::bind(&unicorn_t::subscribe, this, _1, _2));
    on<io::unicorn::put>(std::bind(&unicorn_t::put, this, _1, _2, _3));
    on<io::unicorn::del>(std::bind(&unicorn_t::del, this, _1, _2));
    on<io::unicorn::increment>(std::bind(&unicorn_t::increment, this, _1, _2));
}

unicorn_t::response::put
unicorn_t::put(path_t path, value_t value, version_t version) {
    response::put result;
    auto context = std::make_unique<put_context_t>(
        this,
        std::move(path),
        std::move(value),
        std::move(version),
        result
    );
    auto& context_ref = *context;
    auto handler = std::make_unique<put_action_t>(std::move(context));
    zk.put(context_ref.path, context_ref.value, context_ref.version, std::move(handler));
    return result;
}

unicorn_t::response::del
unicorn_t::del(path_t path, version_t version) {
    response::del result;
    auto handler = std::make_unique<del_action_t>(result);
    zk.del(path, version, std::move(handler));
    return result;
}

unicorn_t::response::subscribe
unicorn_t::subscribe(path_t path, version_t current_version) {
    unicorn_t::response::subscribe result;
    subscribe_context_ptr context = std::make_shared<subscribe_context_t>(
        this,
        std::move(result),
        std::move(path),
        std::move(current_version)
    );
    const auto& path_ref = context->path;
    auto subscribe_handler = std::make_unique<subscribe_action_t>(context);
    auto watch_handler = std::make_unique<watch_handler_t>(std::move(context));
    zk.get(path_ref, std::move(subscribe_handler), std::move(watch_handler));
    return result;
}

unicorn_t::response::increment
unicorn_t::increment(path_t path, value_t value) {
    unicorn_t::response::increment result;
    if (!value.is_double() && !value.is_int() && !value.is_uint()) {
        result.abort(-1, "Non numeric value passed for increment");
        return result;
    }
    increment_context_ptr context = std::make_unique<increment_context_t>(
        this,
        std::move(result),
        std::move(path),
        std::move(value)
    );
    const auto& context_ref = *context;
    auto increment_handler = std::make_unique<increment_get_action_t>(std::move(context));
    zk.get(context_ref.path, std::move(increment_handler));
    return result;
}

}}
