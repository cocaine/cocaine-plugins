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

#include "cocaine/zookeeper/connection.hpp"

#include <cocaine/idl/unicorn.hpp>

namespace cocaine {
namespace unicorn {

class zookeeper_t :
    public api::unicorn_t
{
    cocaine::context_t& context;
    std::unique_ptr<api::executor_t> executor;
    const std::string name;
    const std::unique_ptr<logging::logger_t> log;
    zookeeper::session_t zk_session;
    zookeeper::connection_t zk;

public:
    class put_t;
    class get_t;
    class create_t;
    class del_t;
    class subscribe_t;
    class children_subscribe_t;
    class increment_t;
    class lock_t;

    using callback = api::unicorn_t::callback;
    using scope_ptr = api::unicorn_scope_ptr;

    zookeeper_t(cocaine::context_t& context, const std::string& name, const dynamic_t& args);

    ~zookeeper_t();

    auto put(callback::put callback, const path_t& path, const value_t& value, version_t version) -> scope_ptr override;

    auto get(callback::get callback, const path_t& path) -> scope_ptr override;

    auto create(callback::create callback, const path_t& path, const value_t& value, bool ephemeral, bool sequence)
            -> scope_ptr override;

    auto del(callback::del callback, const path_t& path, version_t version) -> scope_ptr override;

    auto subscribe(callback::subscribe callback, const path_t& path) -> scope_ptr override;

    auto children_subscribe(callback::children_subscribe callback, const path_t& path) -> scope_ptr override;

    auto increment(callback::increment callback, const path_t& path, const value_t& value) -> scope_ptr override;

    auto lock(callback::lock callback, const path_t& path) -> scope_ptr override;

private:
    template<class Action, class Callback, class... Args>
    auto run_command(Callback callback, Args&& ...args) -> scope_ptr;
};

}}
