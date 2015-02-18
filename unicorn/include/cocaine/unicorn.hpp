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

#ifndef COCAINE_UNICORN_SERVICE_HPP
#define COCAINE_UNICORN_SERVICE_HPP

#include "cocaine/idl/unicorn.hpp"
#include "cocaine/unicorn/path.hpp"
#include "cocaine/unicorn/value.hpp"

#include "cocaine/zookeeper/connection.hpp"
#include "cocaine/zookeeper/session.hpp"

#include <cocaine/api/service.hpp>
#include <cocaine/rpc/dispatch.hpp>

#include <asio/ip/tcp.hpp>
#include <asio/deadline_timer.hpp>

using namespace cocaine::unicorn;
namespace cocaine { namespace service {
class unicorn_t:
    public api::service_t,
    public dispatch<io::unicorn_tag>
{
public:
    struct response {
        typedef deferred<result_of<io::unicorn::put>::type> put;
        typedef deferred<result_of<io::unicorn::del>::type> del;
        typedef deferred<result_of<io::unicorn::increment>::type> increment;
        typedef streamed<result_of<io::unicorn::subscribe>::type> subscribe;
    };

    unicorn_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args);

    response::put
    put(path_t path, value_t value, version_t version);

    response::del
    del(path_t path, version_t version);

    response::subscribe
    subscribe(path_t path, version_t upstream_version);

    response::increment
    increment(path_t path, value_t value);

    virtual
    const io::basic_dispatch_t&
    prototype() const {
        return *this;
    }

    struct subscribe_context_t;
    struct subscribe_action_t;
    struct watch_handler_t;

    struct put_context_t;
    struct put_action_t;
    struct put_nonode_action_t;
    struct put_badversion_action_t;

    struct del_action_t;

    struct increment_context_t;
    struct increment_action_t;
    struct increment_get_action_t;
    struct increment_set_action_t;

private:
    zookeeper::session_t zk_session;
    zookeeper::connection_t zk;
    std::shared_ptr<cocaine::logging::log_t> log;
};
}}
#endif
