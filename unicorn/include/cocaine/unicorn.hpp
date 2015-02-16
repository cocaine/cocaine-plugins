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
#include "cocaine/zookeeper/connection.hpp"

#include <cocaine/api/service.hpp>
#include <cocaine/rpc/dispatch.hpp>

#include <asio/ip/tcp.hpp>
#include <asio/deadline_timer.hpp>
#include <cocaine/zookeeper/handler.hpp>


namespace cocaine { namespace service {
class unicorn_t:
    public api::service_t,
    public dispatch<io::unicorn_tag>
{
public:
    struct response {
        typedef deferred<result_of<io::unicorn::put>::type> put;
//        typedef deferred<result_of<io::unicorn::get>::type> get;
        typedef deferred<result_of<io::unicorn::subscribe>::type> subscribe;
//        typedef deferred<result_of<io::unicorn::del>::type> del;
//        typedef deferred<result_of<io::unicorn::compare_and_del>::type> compare_and_del;
    };

    struct put_action_t:
    public std::shared_from_this<put_action_t>, zookeeper::stat_handler_base_t {
        path_t path;
        value_t value;
        response::put result;
        virtual void operator()(int rc, zookeeper::node_stat const& stat) override;
    };

    struct get_action_t:
    public std::shared_from_this<get_action_t>, zookeeper::data_handler_base_t {
        response::get result;
        void operator()(const std::error_code& ec);
    };

    struct subscribe_action_t:
    public std::shared_from_this<subscribe_action_t>, zookeeper::data_handler_base_t {
        response::subscribe result;
        version_t current_version;
        void operator()(int rc, std::string value, const node_stat& stat);
    };

    struct del_action_t:
    public std::shared_from_this<del_action_t> {
        response::del result;
        void operator()(const std::error_code& ec);
    };

    unicorn_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args);

    response::put
    put(path_t path, value_t value);

    /*
    response::get
    get(const path_t& path);
    */

    response::subscribe
    subscribe(const path_t& path, version_t current_version);

    response::del
    del(const path_t& path);

    response::compare_and_del
    compare_and_del(const path_t& path, const version_t& version);

    virtual
    const io::basic_dispatch_t&
    prototype() const {
        return *this;
    }
private:
    zookeeper::connection_t zk;
};
}}
#endif