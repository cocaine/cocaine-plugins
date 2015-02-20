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
#include "cocaine/unicorn/handlers.hpp"
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

unicorn_t::unicorn_t(context_t& context, asio::io_service& _asio, const std::string& name, const dynamic_t& args) :
    service_t(context, _asio, name, args),
    dispatch<io::unicorn_tag>(name),
    zk_session(),
    zk(make_zk_config(args), zk_session),
    log(context.log("unicorn")) {
    using namespace std::placeholders;
    on<io::unicorn::subscribe>(std::bind(&unicorn_t::subscribe, this, _1, _2));
    on<io::unicorn::lsubscribe>(std::bind(&unicorn_t::lsubscribe, this, _1, _2));
    on<io::unicorn::put>(std::bind(&unicorn_t::put, this, _1, _2, _3));
    on<io::unicorn::del>(std::bind(&unicorn_t::del, this, _1, _2));
    on<io::unicorn::increment>(std::bind(&unicorn_t::increment, this, _1, _2));
    on<io::unicorn::lock>(std::make_shared<lock_slot_t>(this));
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
    auto watch_handler = std::make_unique<subscribe_watch_handler_t>(std::move(context));
    zk.get(path_ref, std::move(subscribe_handler), std::move(watch_handler));
    return result;
}

unicorn_t::response::lsubscribe
unicorn_t::lsubscribe(path_t path, version_t current_version) {
    unicorn_t::response::lsubscribe result;
    lsubscribe_context_ptr context = std::make_shared<lsubscribe_context_t>(
        this,
        std::move(result),
        std::move(path),
        std::move(current_version)
    );
    const auto& path_ref = context->path;
    auto subscribe_handler = std::make_unique<lsubscribe_action_t>(context);
    auto watch_handler = std::make_unique<lsubscribe_watch_handler_t>(std::move(context));
    zk.childs(path_ref, std::move(subscribe_handler), std::move(watch_handler));
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

unicorn_t::nonode_action_t::nonode_action_t(put_context_ptr _context, int _depth) :
    context(std::move(_context)),
    depth(_depth)
{}

void
unicorn_t::nonode_action_t::operator()(int rc, std::string value) {
    if (rc == ZOK) {
        if (depth == 0) {
            context->finalize(versioned_value_t(std::move(context->initial_value), version_t()));
        }
        else {
            auto& context_ref = *context;
            auto ptr = std::make_unique<nonode_action_t>(std::move(context), depth - 1);
            if (depth == 1) {
                context_ref.parent->zk.create(context_ref.path, context_ref.value, context_ref.ephemeral, std::move(ptr));
            }
            else {
                context_ref.parent->zk.create(zookeeper::path_parent(context_ref.path, depth - 2), "", false, std::move(ptr));
            }
        }
    }
    else if (rc == ZNONODE) {
        auto& context_ref = *context;
        auto ptr = std::make_unique<nonode_action_t>(std::move(context), depth + 1);
        context_ref.parent->zk.create(zookeeper::path_parent(context_ref.path, depth), "", false, std::move(ptr));
    }
    else {
        context->abort(rc, "Error during node creation subrequest: " + zookeeper::get_error_message(rc));
    }
}

unicorn_t::subscribe_context_base_t::subscribe_context_base_t(unicorn_t* _parent, path_t _path, version_t version) :
    parent(_parent),
    write_lock(),
    last_version(std::move(version)),
    path(std::move(_path)),
    is_aborted(false) {
}

void
unicorn_t::subscribe_context_base_t::abort(int rc, const std::string& message) {
    is_aborted = true;
    on_abort(rc, message);
}

bool
unicorn_t::subscribe_context_base_t::aborted() {
    return is_aborted;
}

unicorn_t::subscribe_context_t::subscribe_context_t(
    unicorn_t* _parent,
    unicorn_t::response::subscribe _result,
    path_t _path,
    version_t _version
) :
    subscribe_context_base_t(_parent, std::move(_path), std::move(_version)),
    result(std::move(_result))
{}

void
unicorn_t::subscribe_context_t::on_abort(int rc, const std::string& message) {
    result.abort(rc, message);
}

unicorn_t::subscribe_action_t::subscribe_action_t(subscribe_context_ptr _context) :
    context(std::move(_context)) {
}

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
            auto watcher = std::make_unique<subscribe_watch_handler_t>(std::move(context));
            context_ref.parent->zk.exists(context_ref.path, std::move(handler), std::move(watcher));
        }
    }
    else if (rc != 0) {
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

unicorn_t::subscribe_nonode_action_t::subscribe_nonode_action_t(subscribe_context_ptr _context) :
    context(std::move(_context)) {
}

void
unicorn_t::subscribe_nonode_action_t::operator()(int rc, zookeeper::node_stat const& stat) {
    //Nothing to do here
}

unicorn_t::subscribe_watch_handler_t::subscribe_watch_handler_t(subscribe_context_ptr _context) :
    context(std::move(_context)) {
}

void
unicorn_t::subscribe_watch_handler_t::operator()(int type, int state, zookeeper::path_t path) {
    if (context->aborted()) {
        return;
    }
    auto& context_ref = *context;
    auto subscribe_handler = std::make_unique<subscribe_action_t>(context);
    auto watch_handler = std::make_unique<subscribe_watch_handler_t>(std::move(context));
    context_ref.parent->zk.get(context_ref.path, std::move(subscribe_handler), std::move(watch_handler));
}

unicorn_t::lsubscribe_context_t::lsubscribe_context_t(unicorn_t* _parent, unicorn_t::response::lsubscribe _result, path_t _path, version_t _version) :
    subscribe_context_base_t(_parent, _path, _version),
    result(std::move(_result))
{}

void
unicorn_t::lsubscribe_context_t::on_abort(int rc, const std::string& message) {
    result.abort(rc, message);
}

unicorn_t::lsubscribe_action_t::lsubscribe_action_t(lsubscribe_context_ptr _context) :
    context(std::move(_context))
{}

void
unicorn_t::lsubscribe_action_t::operator()(int rc, std::vector<std::string> childs, const zookeeper::node_stat& stat) {
    if (rc != 0) {
        context->abort(rc, zookeeper::get_error_message(rc));
    }
    else {
        version_t new_version(stat.cversion);
        std::lock_guard<std::mutex> guard(context->write_lock);
        if (new_version > context->last_version) {
            context->last_version = new_version;
            value_t val;
            context->result.write(std::make_tuple(new_version, childs));
        }
    }
}

unicorn_t::lsubscribe_watch_handler_t::lsubscribe_watch_handler_t(lsubscribe_context_ptr _context) :
    context(std::move(_context))
{}

void
unicorn_t::lsubscribe_watch_handler_t::operator()(int type, int state, zookeeper::path_t path) {
    if (context->aborted()) {
        return;
    }
    auto& context_ref = *context;
    auto lsubscribe_handler = std::make_unique<lsubscribe_action_t>(context);
    auto watch_handler = std::make_unique<lsubscribe_watch_handler_t>(std::move(context));
    context_ref.parent->zk.childs(context_ref.path, std::move(lsubscribe_handler), std::move(watch_handler));
}

unicorn_t::put_context_base_t::put_context_base_t(
    unicorn_t* _parent,
    path_t _path,
    value_t _value,
    version_t _version,
    bool _ephemeral
) :
    parent(_parent),
    path(std::move(_path)),
    initial_value(std::move(_value)),
    value(serialize(initial_value)),
    version(std::move(_version)),
    ephemeral(_ephemeral)
{}

unicorn_t::put_context_t::put_context_t(
    unicorn_t* _parent,
    path_t _path,
    value_t _value,
    version_t _version,
    unicorn_t::response::put _result
) :
    put_context_base_t(_parent, std::move(_path), std::move(_value), std::move(_version), false),
    result(std::move(_result))
{}

void
unicorn_t::put_context_t::finalize(versioned_value_t cur_value) {
    result.write(std::move(cur_value));
}

void
unicorn_t::put_context_t::abort(int rc, const std::string& message) {
    result.abort(rc, message);
}

unicorn_t::put_action_t::put_action_t(put_context_ptr _context) :
    context(std::move(_context)) {
}

void
unicorn_t::put_action_t::operator()(int rc, zookeeper::node_stat const& stat) {
    if (rc == ZNONODE && context->version == -1) {
        auto& context_ref = *context;
        auto ptr = std::make_unique<nonode_action_t>(std::move(context));
        context_ref.parent->zk.create(context_ref.path, context_ref.value, context_ref.ephemeral, std::move(ptr));
    }
    else if(rc == ZNONODE) {
        context->finalize(versioned_value_t(value_t(), -1));
    }
    else if (rc == ZBADVERSION) {
        auto& context_ref = *context;
        auto ptr = std::make_unique<put_badversion_action_t>(std::move(context));
        context_ref.parent->zk.get(context_ref.path, std::move(ptr));
    }
    else if (rc != 0) {
        context->abort(rc, zookeeper::get_error_message(rc));
    }
    else {
        context->finalize(versioned_value_t(context->initial_value, stat.version));
    }
}

unicorn_t::put_badversion_action_t::put_badversion_action_t(put_context_ptr _context) :
    context(std::move(_context)) {
}

void
unicorn_t::put_badversion_action_t::operator()(int rc, std::string value, zookeeper::node_stat const& stat) {
    if (rc) {
        context->abort(rc, "Failed get after version mismatch:" + zookeeper::get_error_message(rc));
    }
    else {
        try {
            context->finalize(versioned_value_t(unserialize(value), stat.version));
        }
        catch (const std::exception& e) {
            rc = zookeeper::ZOO_EXTRA_ERROR::INVALID_VALUE;
            context->abort(rc, "Error during new value get:" + zookeeper::get_error_message(rc, e));
        }
    }
}

unicorn_t::del_action_t::del_action_t(unicorn_t::response::del _result) :
    result(std::move(_result)) {
}

void
unicorn_t::del_action_t::operator()(int rc) {
    if (rc) {
        result.abort(rc, zookeeper::get_error_message(rc));
    }
    else {
        result.write(true);
    }
}

unicorn_t::increment_context_t::increment_context_t(
    unicorn_t* _parent,
    unicorn_t::response::increment _result,
    path_t _path,
    value_t _increment
):
    parent(_parent),
    result(std::move(_result)),
    path(std::move(_path)),
    increment(std::move(_increment)),
    total() {
}

unicorn_t::increment_set_action_t::increment_set_action_t(increment_context_ptr _context) :
    context(std::move(_context)) {
}

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

unicorn_t::increment_get_action_t::increment_get_action_t(increment_context_ptr _context) :
    context(std::move(_context)) {
}

void
unicorn_t::increment_get_action_t::operator()(int rc, zookeeper::value_t value, zookeeper::node_stat const& stat) {
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
        context->parent->zk.create(context_ref.path, context_ref.value, context_ref.ephemeral, std::move(handler));
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

distributed_lock_t::put_ephemeral_context_t::put_ephemeral_context_t(
    unicorn_t* unicorn,
    distributed_lock_t* _parent,
    path_t _path,
    value_t _value,
    version_t _version,
    unicorn_t::response::acquire _result
) :
    put_context_base_t(unicorn, std::move(_path), std::move(_value), std::move(_version), true),
    result(std::move(_result)),
    cur_parent(_parent)
{}

void
distributed_lock_t::put_ephemeral_context_t::finalize(versioned_value_t cur_value) {
    auto ptr = cur_parent->state.synchronize();
    if(ptr->discarded) {
        parent->zk.del(path, -1, nullptr);
    }
    ptr->lock_acquired = true;
    result.write(true);
}

void
distributed_lock_t::put_ephemeral_context_t::abort(int rc, const std::string& message) {
    result.abort(rc, message);
}

lock_slot_t::lock_slot_t(unicorn_t *const parent_):
    parent(parent_)
{}

boost::optional<std::shared_ptr<const lock_slot_t::dispatch_type>>
lock_slot_t::operator()(tuple_type&& args, upstream_type&& upstream)
{
    //At least clang(Apple LLVM version 6.0) do not accept implicit return of shared_ptr
    return boost::optional<std::shared_ptr<const lock_slot_t::dispatch_type>> (
        std::make_shared<const distributed_lock_t>(parent->name(), std::get<0>(args), parent)
    );
}

distributed_lock_t::distributed_lock_t(const std::string& name, path_t _path, unicorn_t* _parent) :
    dispatch<io::unicorn_locked_tag>(name),
    path(_path),
    parent(_parent)
{
    on<io::unicorn::unlock>(std::bind(&distributed_lock_t::discard, this, std::error_code()));
    on<io::unicorn::acquire>(std::bind(&distributed_lock_t::acquire, this));
}

void
distributed_lock_t::discard(const std::error_code& ec) const {
    auto ptr = state.synchronize();
    if(ptr->lock_acquired) {
        parent->zk.del(path, -1, nullptr);
    }
    ptr->discarded = true;
}

unicorn_t::response::acquire
distributed_lock_t::acquire() {
    unicorn_t::response::acquire result;
    auto context = std::make_unique<put_ephemeral_context_t>(parent, this, path, value_t(time(nullptr)), version_t(), result);
    auto context_ref = *context;
    auto handler = std::make_unique<unicorn_t::nonode_action_t>(std::move(context));
    parent->zk.create(path, context_ref.value, context_ref.ephemeral, std::move(handler));
    return result;
}

}}
