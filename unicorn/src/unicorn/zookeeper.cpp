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

#include "cocaine/detail/unicorn/zookeeper.hpp"
#include "cocaine/detail/unicorn/zookeeper/children_subscribe.hpp"
#include "cocaine/detail/unicorn/zookeeper/create.hpp"
#include "cocaine/detail/unicorn/zookeeper/del.hpp"
#include "cocaine/detail/unicorn/zookeeper/increment.hpp"
#include "cocaine/detail/unicorn/zookeeper/lock.hpp"
#include "cocaine/detail/unicorn/zookeeper/put.hpp"
#include "cocaine/detail/unicorn/zookeeper/subscribe.hpp"
#include "cocaine/detail/unicorn/zookeeper/lock_state.hpp"

#include "cocaine/detail/zookeeper/handler.hpp"

#include "cocaine/detail/zookeeper/errors.hpp"

#include "cocaine/service/unicorn.hpp"

#include <cocaine/context.hpp>

#include <cocaine/unicorn/value.hpp>

#include <asio/io_service.hpp>

#include <blackhole/logger.hpp>

#include <memory>

namespace cocaine {
namespace unicorn {

typedef zookeeper_t::writable_ptr writable_ptr;
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
    return zookeeper::cfg_t(endpoints, cfg.at("recv_timeout_ms", 1000u).as_uint(), cfg.at("prefix", "").as_string());
}

struct zk_scope_t:
public api::unicorn_scope_t
{
    zookeeper::handler_scope_t handler_scope;

    /**
     * Does nothing as all work is done in handler_scope dtor.
     */
    virtual void
    close(){}
};

}
typedef api::unicorn_scope_ptr scope_ptr;
zookeeper_t::zookeeper_t(cocaine::context_t& context, const std::string& name, const dynamic_t& args) :
    api::unicorn_t(context, name, args),
    log(context.log(name)),
    zk_session(),
    zk(make_zk_config(args), zk_session)
{
}

zookeeper_t::~zookeeper_t() = default;

scope_ptr
zookeeper_t::put(
    writable_ptr::put result,
    const path_t& path,
    const value_t& value,
    version_t version
) {
    if (version < 0) {
        result->abort(cocaine::error::version_not_allowed);
        return nullptr;
    }
    auto scope = std::make_shared<zk_scope_t>();
    auto& handler = scope->handler_scope.get_handler<put_action_t>(
        context_t({*log, zk}),
        std::move(result),
        path,
        value,
        std::move(version)
    );
    zk.put(handler.path, handler.encoded_value, handler.version, handler);
    return scope;
}

scope_ptr
zookeeper_t::get(writable_ptr::get result, const path_t& path) {
    auto scope = std::make_shared<zk_scope_t>();
    auto& handler = scope->handler_scope.get_handler<subscribe_action_t>(
        std::move(result),
        context_t({*log, zk}),
        path
    );
    zk.get(handler.path, handler);
    return scope;
}

scope_ptr
zookeeper_t::create(writable_ptr::create result, const path_t& path, const value_t& value, bool ephemeral, bool sequence ) {
    auto scope = std::make_shared<zk_scope_t>();
    //Note: There is a possibility to use unmanaged handler.
    auto& handler = scope->handler_scope.get_handler<create_action_t>(
        context_t({*log, zk}),
        std::move(result),
        path,
        value,
        ephemeral,
        sequence
    );
    zk.create(handler.path, handler.encoded_value, handler.ephemeral, handler.sequence, handler);
    return scope;
}

scope_ptr
zookeeper_t::del(writable_ptr::del result, const path_t& path, version_t version) {
    auto handler = std::make_unique<del_action_t>(result);
    zk.del(path, version, std::move(handler));
    return nullptr;
}

scope_ptr
zookeeper_t::subscribe(writable_ptr::subscribe result, const path_t& path) {
    auto scope = std::make_shared<zk_scope_t>();
    auto& handler = scope->handler_scope.get_handler<subscribe_action_t>(
        std::move(result),
        context_t({*log,zk}),
        path
    );
    zk.get(handler.path, handler, handler);
    return scope;
}

scope_ptr
zookeeper_t::children_subscribe(writable_ptr::children_subscribe result, const path_t& path) {
    auto scope = std::make_shared<zk_scope_t>();
    auto& handler = scope->handler_scope.get_handler<children_subscribe_action_t>(
        std::move(result),
        context_t({*log, zk}),
        path
    );
    zk.childs(handler.path, handler, handler);
    return scope;
}

scope_ptr
zookeeper_t::increment(writable_ptr::increment result, const path_t& path, const value_t& value) {
    if (!value.is_double() && !value.is_int() && !value.is_uint()) {
        result->abort(cocaine::error::unicorn_errors::invalid_type);
        return nullptr;
    }
    auto scope = std::make_shared<zk_scope_t>();
    auto& handler = scope->handler_scope.get_handler<increment_action_t>(
        context_t({*log, zk}),
        std::move(result),
        path,
        value
    );
    zk.get(handler.path, handler);
    return scope;
}

scope_ptr
zookeeper_t::lock(writable_ptr::lock result, const path_t& folder) {
    path_t path = folder + "/lock";
    auto lock_state = std::make_shared<lock_state_t>(context_t({*log, zk}));
    auto& handler = lock_state->handler_scope.get_handler<lock_action_t>(
        context_t({*log, zk}),
        lock_state,
        path,
        std::move(folder),
        value_t(time(nullptr)),
        result
    );
    zk.create(path, handler.encoded_value, handler.ephemeral, handler.sequence, handler);
    return lock_state;
}

zookeeper::value_t serialize(const value_t& val) {
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> packer(buffer);
    cocaine::io::type_traits<cocaine::dynamic_t>::pack(packer, val);
    return std::string(buffer.data(), buffer.size());
}

value_t unserialize(const zookeeper::value_t& val) {
    msgpack::object obj;
    std::unique_ptr<msgpack::zone> z(new msgpack::zone());

    msgpack_unpack_return ret = msgpack_unpack(
    val.c_str(), val.size(), nullptr, z.get(),
    reinterpret_cast<msgpack_object*>(&obj)
    );

    //Only strict unparse.
    if(static_cast<msgpack::unpack_return>(ret) != msgpack::UNPACK_SUCCESS) {
        throw std::system_error(cocaine::error::unicorn_errors::invalid_value);
    }
    value_t target;
    cocaine::io::type_traits<cocaine::dynamic_t>::unpack(obj, target);
    return target;
}


}}
