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

#include "cocaine/zookeeper/zookeeper.hpp"

#include <cocaine/context.hpp>
#include <asio/io_service.hpp>

using namespace cocaine::unicorn;

namespace cocaine { namespace service {

namespace {
zookeeper::cfg_t make_zk_config(const dynamic_t& args) {
    const auto& cfg = args.as_object();
    const auto& endpoints_cfg = cfg.at("endpoints", dynamic_t::empty_array).as_array();
    std::vector<zookeeper::cfg_t::endpoint_t> endpoints;
    for(size_t i = 0; i < endpoints_cfg.size(); i++) {
        endpoints.emplace_back(endpoints_cfg[i].as_object().at("host").as_string(), endpoints_cfg[i].as_object().at("port").as_uint());
    }
    if(endpoints.empty()) {
        endpoints.emplace_back("localhost", 2181);
    }
    return zookeeper::cfg_t(endpoints, cfg.at("recv_timeout", 1).as_uint());
}
}

void unicorn_t::put_action_t::operator()(const std::error_code& ec) {
    if(ec) {
        result.abort(ec.value(), ec.message());
    }
    else {
        result.
    }
}

void unicorn_t::get_action_t::operator()(const std::error_code& ec) {

}

void unicorn_t::subscribe_action_t::operator()(const std::error_code& ec) {

}

void unicorn_t::del_action_t::operator()(const std::error_code& ec) {

}

unicorn_t::unicorn_t(context_t& context, asio::io_service& _asio, const std::string& name, const dynamic_t& args) :
    service_t(context, _asio, name, args),
    dispatch<io::unicorn_tag>(name),
    zk(make_zk_config(args))
{
}

response::put
unicorn_t::put(path_t path, value_t value) {
    response::put result;
    auto ptr = std::make_unique<put_action_t>();
    ptr->path = std::move(path);
    ptr->value = std::move(value);
    ptr->result = result;
    zk.put(ptr->path, ptr->value.serialize(), std::move(ptr));
}

response::get
unicorn_t::get(const path_t& path) {
    response::get result;
    auto ptr = std::make_unique<get_action_t>();
    zk.get(path, std::move(ptr));
}

response::subscribe
unicorn_t::subscribe(const path_t& path, const version_t& current_version){

}

response::del
unicorn_t::del(const path_t& path){
}

response::compare_and_del
unicorn_t::compare_and_del(const path_t& path, const version_t& version){
}


void unicorn_t::put_action_t::operator()(int rc, zookeeper::node_stat const& stat) {
    if(rc == ZNONODE) {
        zk.create()
    }
    if(rc != 0) {
        result.abort(rc, zookeeper::get_error_message(rc));
    }
    else {
        result.write(true);
    }
}

void unicorn_t::subscribe_action_t::operator()(int rc, std::string value, zookeeper::node_stat const& stat) {
    if(rc == ZNONODE)
}
}}