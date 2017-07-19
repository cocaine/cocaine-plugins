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

#include "cocaine/unicorn/zookeeper.hpp"
#include "cocaine/service/unicorn.hpp"
#include "cocaine/traits/dynamic.hpp"
#include "cocaine/unicorn/value.hpp"
#include "cocaine/zookeeper.hpp"

#include <cocaine/context.hpp>
#include <cocaine/executor/asio.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/format.hpp>
#include <cocaine/format/vector.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/utility/future.hpp>

#include <asio/io_service.hpp>

#include <blackhole/logger.hpp>

#include <memory>
#include <blackhole/wrapper.hpp>

#include <zookeeper/zookeeper.h>

using namespace cocaine::zookeeper;

namespace cocaine {
namespace unicorn {

namespace {

template<class T>
using future_callback = std::function<void(std::future<T>)>;

cfg_t make_zk_config(const dynamic_t& args) {
    const auto& cfg = args.as_object();
    const auto& endpoints_cfg = cfg.at("endpoints", dynamic_t::empty_array).as_array();
    std::vector<cfg_t::endpoint_t> endpoints;
    for(const auto& ep: endpoints_cfg) {
        endpoints.emplace_back(ep.as_object().at("host").as_string(), ep.as_object().at("port").as_uint());
    }
    if(endpoints.empty()) {
        endpoints.emplace_back("localhost", 2181);
    }
    return cfg_t(endpoints, cfg.at("recv_timeout_ms", 1000u).as_uint(), cfg.at("prefix", "").as_string());
}

auto throw_watch_event(const watch_reply_t& reply) -> void {
    throw error_t(error::connection_loss, "watch occured with \"{}\" type, \"{}\" state event on \"{}\" path",
                  event_to_string(reply.type), state_to_string(reply.state), reply.path);
}

}

class scope_t: public api::unicorn_scope_t {
public:
    // This recursive mutex seems to be best solution to prevent dead locks in bad designed unicorn api.
    // And yes, I will burn in hell for this.
    synchronized<bool, std::recursive_mutex> closed;

    auto close() -> void override {
        closed.apply([&](bool& closed){
            closed = true;
        });
    }

    virtual
    auto on_abort() -> void {}
};

struct abortable_t {
    virtual
    ~abortable_t() {}

    virtual
    auto abort_with_current_exception() -> void = 0;
};

template<class Reply>
class action: public replier<Reply>, public virtual abortable_t {
public:
    action() {}

    auto operator()(Reply reply) -> void override {
        try {
            on_reply(std::move(reply));
        } catch(...) {
            abort_with_current_exception();
        }
    }

private:
    virtual
    auto on_reply(Reply reply) -> void = 0;
};

template <class T, class... Args>
class safe: public std::enable_shared_from_this<safe<T, Args...>>, public action<Args>... {
    std::shared_ptr<scope_t> _scope;
    future_callback<T> wrapped;

public:
    virtual ~safe() {}

    auto scope() -> std::shared_ptr<scope_t> {
        return _scope;
    }

    safe(future_callback<T> wrapped) :
            _scope(std::make_shared<scope_t>()),
            wrapped(std::move(wrapped))
    {}

    safe(std::shared_ptr<scope_t> scope, future_callback<T> wrapped) :
            _scope(std::move(scope)),
            wrapped(std::move(wrapped))
    {}

    template<class Result>
    auto satisfy(Result&& result) -> void {
        _scope->closed.apply([&](bool& closed){
            if(!closed) {
                wrapped(make_ready_future<T>(std::forward<Result>(result)));
            }
        });
    }

    virtual
    auto abort_with_current_exception() -> void override {
        _scope->closed.apply([&](bool& closed){
            _scope->on_abort();
            if(!closed) {
                wrapped(make_exceptional_future<T>());
            }
            closed = true;
        });
    }

    auto abort(std::exception_ptr eptr) -> void {
        try {
            std::rethrow_exception(eptr);
        } catch(...){
            abort_with_current_exception();
        }
    }
};

class zookeeper_t::put_t: public safe<response::put, put_reply_t, get_reply_t>
{
    zookeeper_t& parent;
    path_t path;
    value_t value;
    version_t version;
public:
    put_t(callback::put wrapped, zookeeper_t& parent, path_t path, value_t value, version_t version):
            safe(std::move(wrapped)),
            parent(parent),
            path(std::move(path)),
            value(std::move(value)),
            version(version)
    {}

    auto run() -> void {
        if(version < 0) {
            throw error_t(error::version_not_allowed, "negative version is not allowed for put");
        }
        parent.zk.put(path, serialize(value), version, shared_from_this());
    }

private:
    auto on_reply(put_reply_t reply) -> void override {
        if(reply.rc == ZBADVERSION) {
            parent.zk.get(path, shared_from_this());
        } else if(reply.rc != 0) {
            throw error_t(map_zoo_error(reply.rc), "failure during writing node value - {}", zerror(reply.rc));
        } else {
            satisfy(std::make_tuple(true, versioned_value_t(value, reply.stat.version)));
        }
    }

    auto on_reply(get_reply_t reply) -> void override {
        if (reply.rc) {
            throw error_t(map_zoo_error(reply.rc), "failure during getting new node value - {}", zerror(reply.rc));
        } else {
            satisfy(std::make_tuple(false, versioned_value_t(unserialize(reply.data), reply.stat.version)));
        }
    }
};


class zookeeper_t::get_t: public safe<versioned_value_t, get_reply_t> {
    zookeeper_t& parent;
    path_t path;
public:
    get_t(callback::get wrapped, zookeeper_t& parent, path_t path):
        safe(std::move(wrapped)),
        parent(parent),
        path(std::move(path))
    {}

    auto run() -> void {
        parent.zk.get(path, shared_from_this());
    }

private:
    auto on_reply(get_reply_t reply) -> void override {
        if (reply.rc != 0) {
            throw error_t(map_zoo_error(reply.rc), "failure during getting node value - {}", zerror(reply.rc));
        } else if (reply.stat.numChildren != 0) {
            throw error_t(cocaine::error::child_not_allowed, "trying to read value of the node with childs");
        } else {
            satisfy(versioned_value_t(unserialize(reply.data), reply.stat.version));
        }
    }
};

class zookeeper_t::create_t: public safe<bool, create_reply_t> {
    zookeeper_t& parent;
    path_t path;
    value_t value;
    bool ephemeral;
    bool sequence;
    size_t depth;

public:
    create_t(callback::create wrapped, zookeeper_t& parent, path_t path, value_t value, bool ephemeral, bool sequence) :
            safe(std::move(wrapped)),
            parent(parent),
            path(std::move(path)),
            value(std::move(value)),
            ephemeral(ephemeral),
            sequence(sequence),
            depth(0)
    {}

    auto run() -> void {
        parent.zk.create(path, serialize(value), ephemeral, sequence, shared_from_this());
    }

private:
    auto on_reply(create_reply_t reply) -> void override {
        if(reply.rc == ZOK) {
            if(depth == 0) {
                satisfy(true);
            } else if(depth == 1) {
                depth--;
                parent.zk.create(path, serialize(value), ephemeral, sequence, shared_from_this());
            } else {
                depth--;
                parent.zk.create(path_parent(path, depth), "", false, false, shared_from_this());
            }
        } else if(reply.rc == ZNONODE) {
            depth++;
            parent.zk.create(path_parent(path, depth), "", false, false, shared_from_this());
        } else {
            throw error_t(map_zoo_error(reply.rc), "failure during creating node - {}", zerror(reply.rc));
        }
    }
};

class zookeeper_t::del_t: public safe<bool, del_reply_t> {
    zookeeper_t& parent;
    path_t path;
    version_t version;

public:
    del_t(callback::del wrapped, zookeeper_t& parent, path_t path, version_t version) :
        safe(std::move(wrapped)),
        parent(parent),
        path(std::move(path)),
        version(version)
    {}

    auto run() -> void {
        parent.zk.del(path, version, shared_from_this());
    }

private:
    auto on_reply(del_reply_t reply) -> void override {
        if (reply.rc != 0) {
            throw error_t(map_zoo_error(reply.rc), "failure during deleting node - {}", zerror(reply.rc));
        } else {
            satisfy(true);
        }
    }
};

class zookeeper_t::subscribe_t: public safe<versioned_value_t, exists_reply_t, get_reply_t, watch_reply_t> {
    zookeeper_t& parent;
    path_t path;

public:
    subscribe_t(callback::subscribe wrapped, zookeeper_t& parent, path_t path):
            safe(std::move(wrapped)),
            parent(parent),
            path(std::move(path))
    {}

    auto run() -> void {
        COCAINE_LOG_DEBUG(parent.log, "unicorn subscribe started on {}", path);
        parent.zk.exists(path, shared_from_this(), shared_from_this());
    }

private:
    auto on_reply(get_reply_t reply) -> void override {
        COCAINE_LOG_DEBUG(parent.log, "handling get reply in subscription on {}", path);
        if(reply.rc) {
            throw error_t(map_zoo_error(reply.rc), "node was removed - {}", zerror(reply.rc));
        } else if (reply.stat.numChildren != 0) {
            throw error_t(error::child_not_allowed, "trying to subscribe on node with childs");
        } else {
            satisfy(versioned_value_t(unserialize(reply.data), reply.stat.version));
        }
    }

    auto on_reply(exists_reply_t reply) -> void override {
        COCAINE_LOG_DEBUG(parent.log, "handling exists reply in subscription on {}", path);
        if(reply.rc == ZOK) {
            parent.zk.get(path, shared_from_this(), shared_from_this());
        } else {
            satisfy(versioned_value_t(value_t(), unicorn::not_existing_version));
        }
    }

    auto on_reply(watch_reply_t reply) -> void override {
        COCAINE_LOG_DEBUG(parent.log, "handling watch reply in subscription on {}", path);
        //TODO: is it ok?
        if(scope()->closed.unsafe()){
            return;
        }
        auto type = reply.type;
        auto state = reply.state;
        if(type == ZOO_CREATED_EVENT || type == ZOO_CHANGED_EVENT ||
                (type == ZOO_SESSION_EVENT && state == ZOO_CONNECTED_STATE)
        ) {
            return parent.zk.get(path, shared_from_this(), shared_from_this());
        } else if(type == ZOO_DELETED_EVENT) {
            throw error_t(map_zoo_error(ZNONODE), "node was removed");
        } else if(type == ZOO_CHILD_EVENT) {
            throw error_t(error::child_not_allowed, "child created on watched node");
        }
        throw_watch_event(reply);
    }
};

class zookeeper_t::children_subscribe_t: public safe<response::children_subscribe, children_reply_t, watch_reply_t> {
    zookeeper_t& parent;
    path_t path;

public:
    children_subscribe_t(callback::children_subscribe wrapped, zookeeper_t& parent, path_t path) :
            safe(std::move(wrapped)),
            parent(parent),
            path(std::move(path))
    {}

    auto run() -> void {
        parent.zk.childs(path, shared_from_this(), shared_from_this());
    }

private:
    auto on_reply(children_reply_t reply) -> void override {
        if(reply.rc) {
            throw error_t(map_zoo_error(reply.rc), "can not fetch children - {}", zerror(reply.rc));
        } else {
            satisfy(response::children_subscribe(reply.stat.cversion, std::move(reply.children)));
        }
    }

    auto on_reply(watch_reply_t reply) -> void override {
        //TODO: is it ok?
        if(scope()->closed.unsafe()){
            return;
        }
        if(reply.type == ZOO_DELETED_EVENT) {
            throw error_t(error::no_node, "watched node was deleted");
        } else if(reply.type == ZOO_CHILD_EVENT || (reply.type == ZOO_SESSION_EVENT && reply.state == ZOO_CONNECTED_STATE)) {
            return parent.zk.childs(path, shared_from_this(), shared_from_this());
        } else if(reply.type == ZOO_SESSION_EVENT) {
            throw error_t(error::connection_loss, "session event {} {}, possible disconnection", reply.type, reply.state);
        }
        throw_watch_event(reply);
    }
};

class zookeeper_t::increment_t: public safe<versioned_value_t, get_reply_t, create_reply_t, put_reply_t> {
    zookeeper_t& parent;
    path_t path;
    value_t value;
public:
    increment_t(callback::increment wrapped, zookeeper_t& parent, path_t path, value_t value) :
        safe(std::move(wrapped)),
        parent(parent),
        path(std::move(path)),
        value(std::move(value))
    {}

    auto run() -> void {
        if (!value.is_double() && !value.is_int() && !value.is_uint()) {
            throw error_t(error::unicorn_errors::invalid_type, "invalid value type for increment");
        }
        parent.zk.get(path, shared_from_this());
    }

private:
    auto on_reply(get_reply_t reply) -> void override {
        if(reply.rc == ZNONODE) {
            return parent.zk.create(path, serialize(value), false, false, shared_from_this());
        } else if(reply.rc) {
            throw error_t(map_zoo_error(reply.rc), "failed to get node value - {}", zerror(reply.rc));
        }
        value_t parsed = unserialize(reply.data);
        if (reply.stat.numChildren != 0) {
            throw error_t(error::child_not_allowed, "can not increment node with children");
        } else if (!parsed.is_double() && !parsed.is_int() && !parsed.is_uint()) {
            throw error_t(error::invalid_type, "can not increment non-numeric value");
        } else if (parsed.is_double() || value.is_double()) {
            value = parsed.to<double>() + value.to<double>();
        } else {
            value = parsed.to<int64_t>() + value.to<int64_t>();
        }
        parent.zk.put(path, serialize(value), reply.stat.version, shared_from_this());
    }

    auto on_reply(put_reply_t reply) -> void override {
        if(reply.rc) {
            throw error_t(map_zoo_error(reply.rc), "failed to put new node value - {}", zerror(reply.rc));
        }
        satisfy(versioned_value_t(value, reply.stat.version));
    }

    auto on_reply(create_reply_t reply) -> void override {
        if(reply.rc) {
            throw error_t(map_zoo_error(reply.rc), "could not create value - {}", zerror(reply.rc));
        } else {
            satisfy(versioned_value_t(value, version_t()));
        }
    }
};

class zookeeper_t::lock_t : public safe<bool, create_reply_t, children_reply_t, exists_reply_t, del_reply_t, watch_reply_t> {
public:
    struct lock_scope_t: public scope_t {
        std::shared_ptr<lock_t> parent;

        lock_scope_t() {}

        auto close() -> void override {
            closed.apply([&](bool& closed){
                if(!closed) {
                    on_abort();
                    closed = true;
                }
            });
        }

        auto on_abort() -> void override {
            BOOST_ASSERT(parent);
            if(!parent->created_sequence_node.empty()) {
                auto lock_path = parent->folder + "/" + parent->created_sequence_node;
                try {
                    parent->parent.zk.del(lock_path, parent->shared_from_this());
                } catch(const std::system_error& e) {
                    COCAINE_LOG_ERROR(parent->parent.log, "can not delete lock on {} - {}, reconnecting as a last resort",
                                      lock_path, error::to_string(e));
                    parent->parent.zk.reconnect();
                }
                parent = nullptr;
            }
        }
    };

private:
    zookeeper_t& parent;
    path_t folder;
    path_t path;
    path_t created_sequence_node;
    value_t value;
    uint64_t depth;

public:
    lock_t(callback::lock wrapped, zookeeper_t& parent, path_t folder) :
        safe(std::make_shared<lock_scope_t>(), std::move(wrapped)),
        parent(parent),
        folder(std::move(folder)),
        path(this->folder + "/lock"),
        value(time(nullptr)),
        depth(0)
    {}

    auto run() -> void {
        //TODO: What can we do with this ugly hack with dynamic_cast?
        std::dynamic_pointer_cast<lock_scope_t>(scope())->parent = std::dynamic_pointer_cast<lock_t>(shared_from_this());
        COCAINE_LOG_DEBUG(parent.log, "starting lock operation on path {}, folder {}", path, folder);
        parent.zk.create(path, serialize(value), true, true, shared_from_this());
    }

private:
    auto on_reply(create_reply_t reply) -> void override {
        if(reply.rc == ZOK) {
            if(depth == 0) {
                return scope()->closed.apply([&](bool& closed){
                    created_sequence_node = get_node_name(reply.created_path);
                    if(!closed) {
                        parent.zk.childs(folder, shared_from_this());
                    } else {
                        parent.zk.del(created_sequence_node, shared_from_this());
                    }
                });
            } else if(depth == 1) {
                depth--;
                parent.zk.create(path, serialize(value), true, true, shared_from_this());
            } else {
                depth--;
                parent.zk.create(path_parent(path, depth), "", false, false, shared_from_this());
            }
        } else if(reply.rc == ZNONODE) {
            depth++;
            parent.zk.create(path_parent(path, depth), "", false, false, shared_from_this());
        } else {
            throw error_t(map_zoo_error(reply.rc), "failure during creating node - {}", zerror(reply.rc));
        }
    }

    auto on_reply(children_reply_t reply) -> void override {
        if(reply.rc) {
            throw error_t(map_zoo_error(reply.rc), "failed to get lock folder children - {}", zerror(reply.rc));
        }
        auto& children = reply.children;
        std::sort(children.begin(), children.end());
        COCAINE_LOG_DEBUG(parent.log, "children - {}, created path - {}", children, created_sequence_node);
        auto it_pair = std::equal_range(children.begin(), children.end(), created_sequence_node);
        if(it_pair.first == it_pair.second) {
            throw error_t("created path is not found in children");
        }
        if(it_pair.first == children.begin()) {
            return satisfy(true);
        } else {
            auto previous_it = --it_pair.first;
            if(!is_valid_sequence_node(*previous_it)) {
                throw error_t("trash data in lock folder");
            }
            auto prev_node = folder + "/" + *previous_it;
            parent.zk.exists(prev_node, shared_from_this(), shared_from_this());
        }
    }

    auto on_reply(exists_reply_t reply) -> void override {
        if(reply.rc == ZNONODE) {
            satisfy(true);
        } else if(reply.rc) {
            throw error_t(map_zoo_error(reply.rc), "error during fetching previous lock - {}", zerror(reply.rc));
        }
    }

    auto on_reply(watch_reply_t reply) -> void override {
        if(reply.type == ZOO_DELETED_EVENT) {
            return satisfy(true);
        }
        throw_watch_event(reply);
    }

    auto on_reply(del_reply_t reply) -> void override {
        if(reply.rc) {
            COCAINE_LOG_ERROR(parent.log, "failed to delete lock, reconnecting");
            parent.zk.reconnect();
        }
    }
};


template<class Action, class Callback, class... Args>
auto zookeeper_t::run_command(Callback callback, Args&& ...args) -> scope_ptr {
    auto action = std::make_shared<Action>(std::move(callback), *this, std::forward<Args>(args)...);
    try {
        action->run();
    } catch (...) {
        auto eptr = std::current_exception();
        executor->spawn([=](){
            action->abort(eptr);
        });
    }
    return action->scope();
}

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

auto zookeeper_t::put(callback::put callback, const path_t& path, const value_t& value, version_t version) -> scope_ptr {
    return run_command<put_t>(std::move(callback), path, value, version);
}

auto zookeeper_t::get(callback::get callback, const path_t& path) -> scope_ptr {
    return run_command<get_t>(std::move(callback), path);
}

auto zookeeper_t::create(callback::create callback, const path_t& path, const value_t& value, bool ephemeral, bool sequence) -> scope_ptr {
    return run_command<create_t>(std::move(callback), path, value, ephemeral, sequence);
}

auto zookeeper_t::del(callback::del callback, const path_t& path, version_t version) -> scope_ptr {
    return run_command<del_t>(std::move(callback), path, version);
}

auto zookeeper_t::subscribe(callback::subscribe callback, const path_t& path) -> scope_ptr {
    return run_command<subscribe_t>(std::move(callback), path);
}

auto zookeeper_t::children_subscribe(callback::children_subscribe callback, const path_t& path) -> scope_ptr {
    return run_command<children_subscribe_t>(std::move(callback), path);
}

auto zookeeper_t::increment(callback::increment callback, const path_t& path, const value_t& value) -> scope_ptr {
    return run_command<increment_t>(std::move(callback), path, value);
}

auto zookeeper_t::lock(callback::lock callback, const path_t& path) -> scope_ptr {
    return run_command<lock_t>(std::move(callback), path);
}

}}
