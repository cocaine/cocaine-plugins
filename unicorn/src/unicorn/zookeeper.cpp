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
#include "cocaine/unicorn/value.hpp"
#include "cocaine/service/unicorn.hpp"

#include <cocaine/context.hpp>
#include <cocaine/executor/asio.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/format.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/utility/future.hpp>

#include <asio/io_service.hpp>

#include <blackhole/logger.hpp>

#include <memory>
#include <blackhole/wrapper.hpp>

namespace cocaine {
namespace unicorn {

typedef api::unicorn_t::callback callback;
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

template<class F, class Result>
auto try_run(F runnable, std::function<void(std::future<Result>)>& callback, api::executor_t& executor) -> void {
    typedef std::function<void(std::future<Result>)> callback_t;
    try {
        runnable();
    } catch (...) {
        auto future = make_exceptional_future<Result>();
        executor.spawn(std::bind([](callback_t& cb, std::future<Result>& f) {
            cb(std::move(f));
        }, std::move(callback), std::move(future)));
    }
}
}

typedef api::unicorn_scope_ptr scope_ptr;
zookeeper_t::zookeeper_t(cocaine::context_t& _context, const std::string& _name, const dynamic_t& args) :
    api::unicorn_t(_context, name, args),
    context(_context),
    executor(new cocaine::executor::owning_asio_t()),
    name(_name),
    log(context.log(cocaine::format("unicorn/{}", name))),
    zk_session(),
    zk(make_zk_config(args), zk_session)
{
}

zookeeper_t::~zookeeper_t() = default;

scope_ptr
zookeeper_t::put(
    callback::put callback,
    const path_t& path,
    const value_t& value,
    version_t version
) {
    auto scope = std::make_shared<zk_scope_t>();
    if (version < 0) {
        //TODO: use move capture when it is available
        executor->spawn(std::bind([](callback::put& cb){
            auto ec = make_error_code(cocaine::error::version_not_allowed);
            auto future = make_exceptional_future<response::put>(std::move(ec));
            cb(std::move(future));
        }, std::move(callback)));

        return scope;
    }

    std::shared_ptr<logging::logger_t> logger = context.log(cocaine::format("unicorn/{}", name), {
        {"method", "put"},
        {"path", path},
        {"version", version},
        {"command_id", rand()}});

    auto& handler = scope->handler_scope.get_handler<put_action_t>(
        context_t({logger, zk}),
        std::move(callback),
        path,
        value,
        std::move(version)
    );

    COCAINE_LOG_DEBUG(logger, "start processing");
    try_run([&]{
        zk.put(handler.path, handler.encoded_value, handler.version, handler);
    }, handler.callback, *executor);

    COCAINE_LOG_DEBUG(logger, "enqueued");
    return scope;
}

scope_ptr
zookeeper_t::get(callback::get callback, const path_t& path) {
    auto scope = std::make_shared<zk_scope_t>();
    std::shared_ptr<logging::logger_t> logger = context.log(cocaine::format("unicorn/{}", name), {
        {"method", {"get"}},
        {"path", {path}},
        {"command_id", {rand()}}});
    auto& handler = scope->handler_scope.get_handler<subscribe_action_t>(
        std::move(callback),
        context_t({logger, zk}),
        path
    );
    COCAINE_LOG_DEBUG(logger, "start processing");
    try_run([&]{
        zk.get(handler.path, handler);
    }, handler.callback, *executor);

    COCAINE_LOG_DEBUG(logger, "enqueued");
    return scope;
}

scope_ptr
zookeeper_t::create(callback::create callback, const path_t& path, const value_t& value, bool ephemeral, bool sequence ) {
    auto scope = std::make_shared<zk_scope_t>();
    std::shared_ptr<logging::logger_t> logger = context.log(cocaine::format("unicorn/{}", name), {
        {"method", "create"},
        {"path", path},
        {"ephemeral", ephemeral},
        {"sequence", sequence},
        {"command_id", rand()}});
    //Note: There is a possibility to use unmanaged handler.
    auto& handler = scope->handler_scope.get_handler<create_action_t>(
        context_t({logger, zk}),
        std::move(callback),
        path,
        value,
        ephemeral,
        sequence
    );
    COCAINE_LOG_DEBUG(logger, "start processing");
    try_run([&]{
        zk.create(handler.path, handler.encoded_value, handler.ephemeral, handler.sequence, handler);
    }, handler.callback, *executor);

    COCAINE_LOG_DEBUG(logger, "enqueued");
    return scope;
}

scope_ptr
zookeeper_t::del(callback::del callback, const path_t& path, version_t version) {
    auto handler = std::make_unique<del_action_t>(callback);
    try_run([&]{
        zk.del(path, version, std::move(handler));
    }, callback, *executor);

    return std::make_shared<zk_scope_t>();
}

scope_ptr
zookeeper_t::subscribe(callback::subscribe callback, const path_t& path) {
    std::shared_ptr<logging::logger_t> logger = context.log(cocaine::format("unicorn/{}", name), {
        {"method", "subscribe"},
        {"path", path},
        {"command_id", rand()}});
    auto scope = std::make_shared<zk_scope_t>();
    auto& handler = scope->handler_scope.get_handler<subscribe_action_t>(
        std::move(callback),
        context_t({logger, zk}),
        path
    );
    COCAINE_LOG_DEBUG(logger, "start processing");
    try_run([&]{
        zk.get(handler.path, handler, handler);
    }, handler.callback, *executor);
    COCAINE_LOG_DEBUG(logger, "enqueued");
    return scope;
}

scope_ptr
zookeeper_t::children_subscribe(callback::children_subscribe callback, const path_t& path) {
    std::shared_ptr<logging::logger_t> logger = context.log(cocaine::format("unicorn/{}", name), {
        {"method", "children_subscribe"},
        {"path", path},
        {"command_id", rand()}});
    auto scope = std::make_shared<zk_scope_t>();
    auto& handler = scope->handler_scope.get_handler<children_subscribe_action_t>(
        std::move(callback),
        context_t({logger, zk}),
        path
    );
    COCAINE_LOG_DEBUG(logger, "start processing");
    try_run([&]{
        zk.childs(handler.path, handler, handler);
    }, handler.callback, *executor);
    COCAINE_LOG_DEBUG(logger, "enqueued");
    return scope;
}

scope_ptr
zookeeper_t::increment(callback::increment callback, const path_t& path, const value_t& value) {
    auto scope = std::make_shared<zk_scope_t>();
    if (!value.is_double() && !value.is_int() && !value.is_uint()) {
        executor->spawn(std::bind([](callback::increment& cb){
            auto ec = make_error_code(cocaine::error::unicorn_errors::invalid_type);
            auto future = make_exceptional_future<response::increment>(ec);
            cb(std::move(future));
        }, std::move(callback)));
        return scope;
    }
    std::shared_ptr<logging::logger_t> logger = context.log(cocaine::format("unicorn/{}", name), {
        {"method", "increment"},
        {"path", path},
        {"command_id", rand()}});
    auto& handler = scope->handler_scope.get_handler<increment_action_t>(
        context_t({logger, zk}),
        std::move(callback),
        path,
        value
    );
    COCAINE_LOG_DEBUG(logger, "start processing");
    try_run([&]{
        zk.get(handler.path, handler);
    }, handler.callback, *executor);
    COCAINE_LOG_DEBUG(logger, "enqueued");
    return scope;
}

scope_ptr
zookeeper_t::lock(callback::lock callback, const path_t& folder) {
    path_t path = folder + "/lock";
    std::shared_ptr<logging::logger_t> logger = context.log(cocaine::format("unicorn/{}", name), {
        {"method", "lock"},
        {"folder", folder},
        {"command_id", rand()}});
    auto lock_state = std::make_shared<lock_state_t>(context_t({logger, zk}));

    auto& handler = lock_state->handler_scope.get_handler<lock_action_t>(
        context_t({logger, zk}),
        lock_state,
        path,
        std::move(folder),
        value_t(time(nullptr)),
        std::move(callback)
    );
    COCAINE_LOG_DEBUG(logger, "start processing");
    try_run([&]{
        zk.create(path, handler.encoded_value, handler.ephemeral, handler.sequence, handler);
    }, handler.callback, *executor);
    COCAINE_LOG_DEBUG(logger, "enqueued");
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
