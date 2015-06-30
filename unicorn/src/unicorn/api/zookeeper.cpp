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
#include "cocaine/unicorn/errors.hpp"
#include "cocaine/unicorn/value.hpp"
#include "cocaine/unicorn/api.hpp"
#include "cocaine/unicorn/handlers.hpp"
#include "cocaine/unicorn/api/zookeeper.hpp"

#include "cocaine/zookeeper/handler.hpp"
#include "cocaine/zookeeper/exception.hpp"

#include <cocaine/context.hpp>
#include <asio/io_service.hpp>
#include <cocaine/zookeeper/exception.hpp>

#include <memory>

namespace cocaine {
namespace unicorn {

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
    return zookeeper::cfg_t(endpoints, cfg.at("recv_timeout_ms", 1000u).as_uint());
}

zookeeper_api_t::zookeeper_api_t(cocaine::logging::log_t& _log, zookeeper::connection_t& _zk) :
    handler_scope(std::make_shared<zookeeper::handler_scope_t>()),
    ctx({_log, _zk})
{
}

void
zookeeper_api_t::put(
    writable_helper<response::put_result>::ptr result,
    path_t path,
    value_t value,
    version_t version
) {
    if (version < 0) {
        result->abort(cocaine::error::VERSION_NOT_ALLOWED);
        return;
    }
    auto& handler = handler_scope->get_handler<put_action_t>(
        ctx,
        result,
        std::move(path),
        std::move(value),
        std::move(version)
    );
    ctx.zk.put(handler.path, handler.encoded_value, handler.version, handler);
}

void
zookeeper_api_t::get(
    writable_helper<response::get_result>::ptr result,
    path_t path
) {
    auto& handler = handler_scope->get_handler<subscribe_action_t>(std::move(result), ctx, std::move(path));
    ctx.zk.get(handler.path, handler);
}

void
zookeeper_api_t::create(
    writable_helper<response::create_result>::ptr result,
    path_t path,
    value_t value,
    bool ephemeral,
    bool sequence
) {
    //Note: There is a possibility to use unmanaged handler.
    auto& handler = handler_scope->get_handler<create_action_t>(
        ctx,
        result,
        std::move(path),
        std::move(value),
        ephemeral,
        sequence
    );
    ctx.zk.create(handler.path, handler.encoded_value, handler.ephemeral, handler.sequence, handler);
}

void
zookeeper_api_t::del(
    writable_helper<response::del_result>::ptr result,
    path_t path,
    version_t version
) {
    auto handler = std::make_unique<del_action_t>(result);
    ctx.zk.del(path, version, std::move(handler));
}

void
zookeeper_api_t::subscribe(
    writable_helper<response::subscribe_result>::ptr result,
    path_t path
) {
    auto& handler = handler_scope->get_handler<subscribe_action_t>(result, ctx, std::move(path));
    ctx.zk.get(handler.path, handler, handler);
}

void
zookeeper_api_t::children_subscribe(
    writable_helper<response::children_subscribe_result>::ptr result,
    path_t path
) {
    auto& handler = handler_scope->get_handler<children_subscribe_action_t>(result, ctx, std::move(path));
    ctx.zk.childs(handler.path, handler, handler);
}

void
zookeeper_api_t::increment(
    writable_helper<response::increment_result>::ptr result,
    path_t path,
    value_t value
) {
    if (!value.is_double() && !value.is_int() && !value.is_uint()) {
        result->abort(cocaine::error::unicorn_errors::INVALID_TYPE);
        return;
    }
    auto& handler = handler_scope->get_handler<increment_action_t>(
        ctx,
        std::move(result),
        std::move(path),
        std::move(value),
        handler_scope
    );
    ctx.zk.get(handler.path, handler);
}

void
zookeeper_api_t::lock(
    writable_helper<response::lock_result>::ptr result,
    path_t path
) {
    path_t folder = std::move(path);
    path = folder + "/lock";
    assert(!lock_state);
    lock_state = std::make_shared<lock_state_t>(ctx);
    lock_state->schedule_for_lock(handler_scope);
    auto& handler = handler_scope->get_handler<lock_action_t>(ctx, lock_state, path, std::move(folder), value_t(time(nullptr)), result);
    ctx.zk.create(path, handler.encoded_value, handler.ephemeral, handler.sequence, handler);
}

void
zookeeper_api_t::close() {
    if(lock_state) {
        lock_state->release();
    }
}

/**************************************************
****************    PUT    ************************
**************************************************/


zookeeper_api_t::put_action_t::put_action_t(
    const zookeeper::handler_tag& tag,
    const zookeeper_api_t::context_t& _ctx,
    writable_helper<response::put_result>::ptr _result,
    path_t _path,
    value_t _value,
    version_t _version
):
    managed_handler_base_t(tag),
    managed_stat_handler_base_t(tag),
    managed_data_handler_base_t(tag),
    ctx(_ctx),
    result(std::move(_result)),
    path(std::move(_path)),
    initial_value(std::move(_value)),
    encoded_value(serialize(initial_value)),
    version(std::move(_version))
{}

void
zookeeper_api_t::put_action_t::operator()(int rc, zookeeper::node_stat const& stat) {
    try {
        if (rc == ZBADVERSION) {
            ctx.zk.get(path, *this);
        } else if (rc != 0) {
            auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
            result->abort(code);
        } else {
            result->write(std::make_tuple(true, versioned_value_t(initial_value, stat.version)));
        }
    } catch (const std::system_error& e) {
        COCAINE_LOG_WARNING(ctx.log, "failure during put action(%i): %s", e.code().value(), e.what());
    }
}

void
zookeeper_api_t::put_action_t::operator()(int rc, zookeeper::value_t value, zookeeper::node_stat const& stat) {
    if (rc) {
        auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
        result->abort(code, "failed get after version mismatch");
    } else {
        try {
            result->write(std::make_tuple(false,versioned_value_t(unserialize(value), stat.version)));
        } catch (const std::system_error& e) {
            result->abort(e.code(), "error during new value get");
        }
    }
}


/**************************************************
*******************  CREATE  **********************
**************************************************/


zookeeper_api_t::create_action_base_t::create_action_base_t(
    const zookeeper::handler_tag& tag,
    const zookeeper_api_t::context_t& _ctx,
    path_t _path,
    value_t _value,
    bool _ephemeral,
    bool _sequence
):
    zookeeper::managed_handler_base_t(tag),
    zookeeper::managed_string_handler_base_t(tag),
    depth(0),
    ctx(_ctx),
    path(std::move(_path)),
    initial_value(std::move(_value)),
    encoded_value(serialize(initial_value)),
    ephemeral(_ephemeral),
    sequence(_sequence)
{}

void
zookeeper_api_t::create_action_base_t::operator()(int rc, zookeeper::value_t value) {
    try {
        if (rc == ZOK) {
            if (depth == 0) {
                finalize(std::move(value));
            } else if (depth == 1) {
                depth--;
                ctx.zk.create(path, encoded_value, ephemeral, sequence, *this);
            } else {
                depth--;
                ctx.zk.create(zookeeper::path_parent(path, depth), "", false, false, *this);
            }
        } else if (rc == ZNONODE) {
            depth++;
            ctx.zk.create(zookeeper::path_parent(path, depth), "", false, false, *this);
        } else {
            abort(rc);
        }
    } catch(const std::system_error& e) {
        COCAINE_LOG_WARNING(ctx.log, "could not create node hierarchy. Exception: %s", e.what());
        abort(e.code().value());
    }
}


zookeeper_api_t::create_action_t::create_action_t(
    const zookeeper::handler_tag& tag,
    const zookeeper_api_t::context_t& _ctx,
    writable_helper<response::create_result>::ptr _result,
    path_t _path,
    value_t _value,
    bool _ephemeral,
    bool _sequence
):
    zookeeper::managed_handler_base_t(tag),
    create_action_base_t(tag, _ctx, std::move(_path), std::move(_value), _ephemeral, _sequence),
    result(std::move(_result))
{}

void
zookeeper_api_t::create_action_t::finalize(zookeeper::value_t) {
    result->write(true);
}

void
zookeeper_api_t::create_action_t::abort(int rc) {
    auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
    result->abort(code);
}



/**************************************************
****************    DEL    ************************
**************************************************/

zookeeper_api_t::del_action_t::del_action_t(writable_helper<response::del_result>::ptr _result) :
    result(std::move(_result)) {
}

void
zookeeper_api_t::del_action_t::operator()(int rc) {
    if (rc) {
        auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
        result->abort(code);
    } else {
        result->write(true);
    }
}




/**************************************************
*****************  SUBSCRIBE   ********************
**************************************************/

zookeeper_api_t::subscribe_action_t::subscribe_action_t(
    const zookeeper::handler_tag& tag,
    writable_helper<response::subscribe_result>::ptr _result,
    const zookeeper_api_t::context_t& _ctx,
    path_t _path
) :
    managed_handler_base_t(tag),
    managed_data_handler_base_t(tag),
    managed_watch_handler_base_t(tag),
    managed_stat_handler_base_t(tag),
    result(std::move(_result)),
    ctx(_ctx),
    write_lock(),
    last_version(unicorn::MIN_VERSION),
    path(std::move(_path))
{}

void
zookeeper_api_t::subscribe_action_t::operator()(int rc, std::string value, const zookeeper::node_stat& stat) {
    if(rc == ZNONODE) {
        if(last_version != MIN_VERSION && last_version != NOT_EXISTING_VERSION) {
            auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
            result->abort(code);
        } else {
            // Write that node is not exist to client only first time.
            // After that set a watch to see when it will appear
            if(last_version == MIN_VERSION) {
                std::lock_guard<std::mutex> guard(write_lock);
                if (NOT_EXISTING_VERSION > last_version) {
                    result->write(versioned_value_t(value_t(), NOT_EXISTING_VERSION));
                }
            }
            try {
                ctx.zk.exists(path, *this, *this);
            } catch(const std::system_error& e) {
                COCAINE_LOG_WARNING(ctx.log, "failure during subscription(get): %s", e.what());
                result->abort(e.code());
            }
        }
    } else if (rc != 0) {
        auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
        result->abort(code);
    } else if (stat.numChildren != 0) {
        result->abort(cocaine::error::CHILD_NOT_ALLOWED);
    } else {
        version_t new_version(stat.version);
        std::lock_guard<std::mutex> guard(write_lock);
        if (new_version > last_version) {
            last_version = new_version;
            value_t val;
            try {
                result->write(versioned_value_t(unserialize(value), new_version));
            } catch(const std::system_error& e) {
                result->abort(e.code());
            }
        }
    }
}

void
zookeeper_api_t::subscribe_action_t::operator()(int rc, zookeeper::node_stat const&) {
    // Someone created a node in a gap between
    // we received nonode and issued exists
    if(rc == ZOK) {
        try {
            ctx.zk.get(path, *this, *this);
        } catch(const std::system_error& e)  {
            COCAINE_LOG_WARNING(ctx.log, "failure during subscription(stat): %s", e.what());
            result->abort(e.code());
        }
    }
}

void
zookeeper_api_t::subscribe_action_t::operator()(int /* type */, int /* state */, zookeeper::path_t) {
    try {
        ctx.zk.get(path, *this, *this);
    } catch(const std::system_error& e)  {
        result->abort(e.code());
        COCAINE_LOG_WARNING(ctx.log, "failure during subscription(watch): %s", e.what());
    }
}


/**************************************************
*****************   LSUBSCRIBE   ******************
**************************************************/


zookeeper_api_t::children_subscribe_action_t::children_subscribe_action_t(
    const zookeeper::handler_tag& tag,
    writable_helper<response::children_subscribe_result>::ptr _result,
    const zookeeper_api_t::context_t& _ctx,
    path_t _path
) :
    managed_handler_base_t(tag),
    managed_strings_stat_handler_base_t(tag),
    managed_watch_handler_base_t(tag),
    result(std::move(_result)),
    ctx(_ctx),
    write_lock(),
    last_version(unicorn::MIN_VERSION),
    path(std::move(_path))
{}


void
zookeeper_api_t::children_subscribe_action_t::operator()(int rc, std::vector<std::string> childs, const zookeeper::node_stat& stat) {
    if (rc != 0) {
        auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
        result->abort(code);
    } else {
        version_t new_version(stat.cversion);
        std::lock_guard<std::mutex> guard(write_lock);
        if (new_version > last_version) {
            last_version = new_version;
            value_t val;
            result->write(std::make_tuple(new_version, childs));
        }
    }
}

void
zookeeper_api_t::children_subscribe_action_t::operator()(int /*type*/, int /*state*/, zookeeper::path_t /*path*/) {
    try {
        ctx.zk.childs(path, *this, *this);
    } catch(const std::system_error& e) {
        result->abort(e.code());
        COCAINE_LOG_WARNING(ctx.log, "failure during subscription for childs: %s", e.what());
    }
}


/**************************************************
*****************   INCREMENT   *******************
**************************************************/


zookeeper_api_t::increment_action_t::increment_action_t(
    const zookeeper::handler_tag& tag,
    const zookeeper_api_t::context_t& _ctx,
    writable_helper<response::increment_result>::ptr _result,
    path_t _path,
    value_t _increment,
    const std::shared_ptr<zookeeper::handler_scope_t>& _scope
):
    managed_handler_base_t(tag),
    create_action_base_t(tag, _ctx, std::move(_path), std::move(_increment), false, false),
    managed_stat_handler_base_t(tag),
    managed_data_handler_base_t(tag),
    ctx(_ctx),
    result(std::move(_result)),
    total(),
    scope(_scope)
{}


void zookeeper_api_t::increment_action_t::operator()(int rc, zookeeper::node_stat const& stat) {
    if (rc == ZOK) {
        result->write(versioned_value_t(total, stat.version));
    } else if (rc == ZBADVERSION) {
        try {
            ctx.zk.get(path, *this);
        } catch(const std::system_error& e) {
            result->abort(e.code());
            COCAINE_LOG_WARNING(ctx.log, "failure during increment get: %s", e.what());
        }
    }
}

void
zookeeper_api_t::increment_action_t::operator()(int rc, zookeeper::value_t value, const zookeeper::node_stat& stat) {
    try {
        if(rc == ZNONODE) {
            ctx.zk.create(path, encoded_value, ephemeral, sequence, *this);
        } else if (rc != ZOK) {
            auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
            result->abort(code, "increment error during value get");
        } else {
            value_t parsed;
            if (!value.empty()) {
                parsed = unserialize(value);
            }
            if (stat.numChildren != 0) {
                result->abort(cocaine::error::CHILD_NOT_ALLOWED, "increment error during value get");
                return;
            }
            if (!parsed.is_double() && !parsed.is_int() && !parsed.is_uint()) {
                result->abort(cocaine::error::INVALID_TYPE, "increment error during value get");
                return;
            }
            assert(initial_value.is_uint() || initial_value.is_uint() || initial_value.is_double());
            if (parsed.is_double() || initial_value.is_double()) {
                total = parsed.to<double>() + initial_value.to<double>();
                ctx.zk.put(path, serialize(total), stat.version, *this);
            } else {
                total = parsed.to<int64_t>() + initial_value.to<int64_t>();
                ctx.zk.put(path, serialize(total), stat.version, *this);
            }
        }
    } catch(const std::system_error& e) {
        COCAINE_LOG_WARNING(ctx.log, "failure during get action of increment: %s", e.what());
        result->abort(e.code());
    }
}

void
zookeeper_api_t::increment_action_t::finalize(zookeeper::value_t) {
    result->write(versioned_value_t(initial_value, version_t()));
}

void
zookeeper_api_t::increment_action_t::abort(int rc) {
    auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
    result->abort(code);
}

zookeeper_api_t::lock_action_t::lock_action_t(
    const zookeeper::handler_tag& tag,
    const zookeeper_api_t::context_t& _ctx,
    std::shared_ptr<zookeeper_api_t::lock_state_t> _state,
    path_t _path,
    path_t _folder,
    value_t _value,
    writable_helper<response::lock_result>::ptr _result
) :
    zookeeper::managed_handler_base_t(tag),
    zookeeper_api_t::create_action_base_t(tag, _ctx, std::move(_path), std::move(_value), true, true),
    zookeeper::managed_strings_stat_handler_base_t(tag),
    zookeeper::managed_stat_handler_base_t(tag),
    zookeeper::managed_watch_handler_base_t(tag),
    state(std::move(_state)),
    result(std::move(_result)),
    folder(std::move(_folder))
{}

void
zookeeper_api_t::lock_action_t::operator()(int rc, std::vector<std::string> childs, zookeeper::node_stat const& /*stat*/) {
    /**
    * Here we assume:
    *  * nobody has written trash data(any data except locks) in specified directory
    *  * sequence number issued by zookeeper is comaparable by string comparision.
    *
    *  First is responsibility of clients.
    *  Second is true for zookeeper at least 3.4.6,
    */
    if(rc) {
        auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
        COCAINE_LOG_ERROR(ctx.log, "Could not subscribe for lock(%i). : %s", code.value(), code.message().c_str());
        result->abort(code);
        return;
    }
    path_t next_min_node = created_node_name;
    for(size_t i = 0; i < childs.size(); i++) {
        /**
        * Skip most obvious "trash" data.
        */
        if(!zookeeper::is_valid_sequence_node(childs[i])) {
            continue;
        }
        if(childs[i] < next_min_node) {
            if(next_min_node == created_node_name) {
                next_min_node.swap(childs[i]);
            } else {
                if(childs[i] > next_min_node) {
                    next_min_node.swap(childs[i]);
                }
            }
        }
    }
    if(next_min_node == created_node_name) {
        if(!state->release_if_discarded()) {
            result->write(true);
        }
        return;
    } else {
        try {
            ctx.zk.exists(folder + "/" + next_min_node, *this, *this);
        } catch(const std::system_error& e) {
            result->abort(e.code());
            state->release();
        }
    }
}

void zookeeper_api_t::lock_action_t::operator()(int rc, zookeeper::node_stat const& /*stat*/) {
    /* If next lock in queue disappeared during request - issue childs immediately. */
    if(rc == ZNONODE) {
        try {
            ctx.zk.childs(folder, *this);
        } catch(const std::system_error& e) {
            result->abort(e.code());
            state->release();
        }
    }
}

void zookeeper_api_t::lock_action_t::operator()(int /*type*/, int /*zk_state*/, path_t /*path*/) {
    try {
        ctx.zk.childs(folder, *this);
    } catch(const std::system_error& e) {
        result->abort(e.code());
        state->release();
    }
}

void
zookeeper_api_t::lock_action_t::finalize(zookeeper::value_t value) {
    zookeeper::path_t created_path = std::move(value);
    created_node_name = zookeeper::get_node_name(created_path);
    if(state->set_lock_created(std::move(created_path))) {
        try {
            ctx.zk.childs(folder, *this);
        } catch(const std::system_error& e) {
            result->abort(e.code());
            state->release();
        }
    }
}

void
zookeeper_api_t::lock_action_t::abort(int rc) {
    auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
    result->abort(code);
}

zookeeper_api_t::release_lock_action_t::release_lock_action_t(const zookeeper_api_t::context_t& _ctx) :
    ctx(_ctx)
{}

void
zookeeper_api_t::release_lock_action_t::operator()(int rc) {
    if(rc) {
        auto code = cocaine::error::make_error_code(static_cast<cocaine::error::zookeeper_errors>(rc));
        COCAINE_LOG_WARNING(ctx.log, "ZK error during lock delete: %s. Reconnecting to discard lock for sure.", code.message().c_str());
        ctx.zk.reconnect();
    }
}

zookeeper_api_t::lock_state_t::lock_state_t(const zookeeper_api_t::context_t& _ctx) :
    ctx(_ctx),
    lock_created(false),
    lock_released(false),
    discarded(false),
    created_path(),
    access_mutex(),
    zk_scope()
{}

zookeeper_api_t::lock_state_t::~lock_state_t(){
    release();
}

void zookeeper_api_t::lock_state_t::schedule_for_lock(scope_ptr _zk_scope) {
    zk_scope = std::move(_zk_scope);
}

void
zookeeper_api_t::lock_state_t::release() {
    bool should_release = false;
    {
        std::lock_guard<std::mutex> guard(access_mutex);
        should_release = lock_created && !lock_released;
        lock_released = lock_released || should_release;
    }
    if(should_release) {
        release_impl();
    }
}

bool
zookeeper_api_t::lock_state_t::release_if_discarded() {
    bool should_release = false;
    {
        std::lock_guard<std::mutex> guard(access_mutex);
        if(discarded && lock_created && !lock_released) {
            should_release = true;
            lock_released = true;
        }
    }
    if(should_release) {
        release_impl();
    }
    return lock_released;
}

void
zookeeper_api_t::lock_state_t::discard() {
    bool should_release = false;
    {
        std::lock_guard<std::mutex> guard(access_mutex);
        discarded = true;
        should_release = lock_created && !lock_released;
        lock_released = lock_released || should_release;
    }
    if(should_release) {
        release_impl();
    }
}

bool
zookeeper_api_t::lock_state_t::set_lock_created(path_t _created_path) {
    //Safe as there is only one call to this
    created_path = std::move(_created_path);
    COCAINE_LOG_DEBUG(ctx.log, "created lock: %s", created_path.c_str());
    //Can not be removed before created.
    assert(!lock_released);
    bool should_release = false;
    {
        std::lock_guard<std::mutex> guard(access_mutex);
        lock_created = true;
        if(discarded) {
            lock_released = true;
            should_release = true;
        }
    }
    if(should_release) {
        release_impl();
    }
    zk_scope.reset();
    return !lock_released;
}

void
zookeeper_api_t::lock_state_t::abort_lock_creation() {
    zk_scope.reset();
}

void
zookeeper_api_t::lock_state_t::release_impl() {
    COCAINE_LOG_DEBUG(ctx.log, "release lock: %s", created_path.c_str());
    try {
        std::unique_ptr<release_lock_action_t> h(new release_lock_action_t(ctx));
        ctx.zk.del(created_path, NOT_EXISTING_VERSION, std::move(h));
    } catch(const std::system_error& e) {
        COCAINE_LOG_WARNING(ctx.log, "ZK exception during delete of lock: %s. Reconnecting to discard lock for sure.", e.what());
        try {
            ctx.zk.reconnect();
        } catch(const std::system_error& e2) {
            COCAINE_LOG_WARNING(ctx.log, "give up on deleting lock. Exception: %s", e2.what());
        }
    }
}

}}
