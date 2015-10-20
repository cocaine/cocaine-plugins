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

#include "cocaine/cluster/unicorn.hpp"

#include <cocaine/rpc/actor.hpp>
#include <cocaine/context.hpp>

namespace cocaine {

template<>
struct dynamic_converter<asio::ip::tcp::endpoint> {
    typedef asio::ip::tcp::endpoint result_type;

    static const bool enable = true;

    static inline
    result_type
    convert(const dynamic_t& from) {
        if (!from.is_array() || from.as_array().size() != 2) {
            throw std::runtime_error("invalid dynamic value for endpoint deserialization");
        }

        asio::ip::tcp::endpoint result(
            asio::ip::address::from_string(from.as_array()[0].to<std::string>()),
            from.as_array()[1].to<unsigned short int>()
        );
        return result;
    }

    static inline
    bool
    convertible(const dynamic_t& from) {
        return (
            from.is_array() &&
            from.as_array().size() == 2 &&
            from.as_array()[0].is_string() &&
            (from.as_array()[1].is_uint() || from.as_array()[1].is_int())
        );
    }
};

template<>
struct dynamic_constructor<asio::ip::tcp::endpoint> {
    static const bool enable = true;

    static inline
    void
    convert(const asio::ip::tcp::endpoint from, dynamic_t::value_t& to) {
        dynamic_constructor<dynamic_t::array_t>::convert(dynamic_t::array_t(), to);
        auto& array = boost::get<detail::dynamic::incomplete_wrapper<dynamic_t::array_t>>(to).get();
        array.resize(2);
        array[0] = from.address().to_string();
        array[1] = from.port();
    }
};

namespace cluster {

unicorn_cluster_t::cfg_t::cfg_t(const dynamic_t& args) :
    path(args.as_object().at("path", "/cocaine/discovery").as_string()),
    retry_interval(args.as_object().at("retry_interval", 1u).as_uint()),
    check_interval(args.as_object().at("check_interval", 60u).as_uint())
{}

unicorn_cluster_t::on_announce::on_announce(unicorn_cluster_t* _parent) :
    parent(_parent)
{}

void
unicorn_cluster_t::on_announce::write(unicorn::api_t::response::create_result&& /*result*/) {
    COCAINE_LOG_INFO(parent->log, "announced self in unicorn");
    parent->unicorn->subscribe(std::make_shared<on_update>(parent), parent->config.path + '/' + parent->locator.uuid());
}

void
unicorn_cluster_t::on_announce::abort(const std::error_code& rc) {
    //Ok for us.
    if(rc.value() == ZNODEEXISTS) {
        COCAINE_LOG_INFO(parent->log, "announce checked");
        return;
    }
    COCAINE_LOG_ERROR(parent->log, "could not announce local services(%i): %s", rc.value(), rc.message());
    parent->announce_timer.expires_from_now(boost::posix_time::seconds(parent->config.retry_interval));
    parent->announce_timer.async_wait(std::bind(&unicorn_cluster_t::on_announce_timer, parent, std::placeholders::_1));
}

unicorn_cluster_t::on_update::on_update(unicorn_cluster_t* _parent) :
    parent(_parent)
{}

void
unicorn_cluster_t::on_update::write(unicorn::api_t::response::subscribe_result&& result) {
    //Node disappeared
    if(result.get_version() < 0) {
        COCAINE_LOG_ERROR(parent->log, "announce dissappeared, retrying.");
        parent->announce();
    }
}

void
unicorn_cluster_t::on_update::abort(const std::error_code& rc) {
    COCAINE_LOG_ERROR(parent->log, "announce dissappeared(%i) : %s, retrying.", rc.value(), rc.message().c_str());
    parent->announce();
}

unicorn_cluster_t::on_fetch::on_fetch(std::string _uuid, unicorn_cluster_t* _parent) :
    uuid(std::move(_uuid)),
    parent(_parent)
{}

void
unicorn_cluster_t::on_fetch::write(unicorn::api_t::response::get_result&& result) {
    if(!result.get_value().convertible_to<std::vector<asio::ip::tcp::endpoint>>()) {
        COCAINE_LOG_WARNING(parent->log, "invalid value for announce: %s",
            boost::lexical_cast<std::string>(result.get_value()).c_str()
        );
        auto storage = parent->registered_locators.synchronize();
        auto it = storage->find(uuid);
        if(it != storage->end()) {
            storage->erase(it);
        }
        return;
    }
    auto storage = parent->registered_locators.synchronize();
    auto it = storage->find(uuid);
    if(it == storage->end()) {
        auto ep_vec = result.get_value().to<std::vector<asio::ip::tcp::endpoint>>();
        parent->locator.link_node(uuid, result.get_value().to<std::vector<asio::ip::tcp::endpoint>>());
        storage->insert(uuid);
    }
}

void
unicorn_cluster_t::on_fetch::abort(const std::error_code& rc) {
    COCAINE_LOG_WARNING(parent->log, "error during fetch(%i): %s", rc.value(), rc.message().c_str());
    auto storage = parent->registered_locators.synchronize();
    auto it = storage->find(uuid);
    bool drop = false;
    if(it != storage->end()) {
        storage->erase(it);
        drop = true;
    }
    if(drop) {
        parent->locator.drop_node(uuid);
    }
}

unicorn_cluster_t::on_list_update::on_list_update(unicorn_cluster_t* _parent) :
parent(_parent)
{}

void
unicorn_cluster_t::on_list_update::write(unicorn::api_t::response::children_subscribe_result&& result) {
    auto nodes = std::get<1>(result);
    std::sort(nodes.begin(), nodes.end());
    std::vector<std::string> to_delete, to_add;
    auto storage = parent->registered_locators.synchronize();
    std::set_difference(nodes.begin(), nodes.end(), storage->begin(), storage->end(), std::back_inserter(to_add));
    std::set_difference(storage->begin(), storage->end(), nodes.begin(), nodes.end(), std::back_inserter(to_delete));
    for(size_t i = 0; i < to_delete.size(); i++) {
        parent->locator.drop_node(to_delete[i]);
        storage->erase(to_delete[i]);
    }
    for(size_t i = 0; i < to_add.size(); i++) {
        if(to_add[i] == parent->locator.uuid()) {
            continue;
        }
        parent->unicorn->get(
            std::make_shared<on_fetch>(to_add[i], parent),
            parent->config.path + '/' + to_add[i]
        );
    }
}

void
unicorn_cluster_t::on_list_update::abort(const std::error_code& rc) {
    COCAINE_LOG_WARNING(parent->log, "failure during subscription(%i): %s, resubscribing", rc.value(), rc.message().c_str());
    parent->subscribe_timer.expires_from_now(boost::posix_time::seconds(parent->config.retry_interval));
    parent->subscribe_timer.async_wait(std::bind(&unicorn_cluster_t::on_subscribe_timer, parent, std::placeholders::_1));

}

unicorn_cluster_t::unicorn_cluster_t(
    cocaine::context_t & _context,
    cocaine::api::cluster_t::interface & _locator,
    const std::string& name,
    const cocaine::dynamic_t& args
):
    cluster_t(_context, _locator, name, args),
    log(_context.log("unicorn")),
    config(args),
    context(_context),
    locator(_locator),
    announce_timer(_locator.asio()),
    subscribe_timer(_locator.asio()),
    zk_session(),
    zk(unicorn::make_zk_config(args), zk_session),
    unicorn(*log, zk)
{
    subscribe_timer.expires_from_now(boost::posix_time::seconds(config.retry_interval));
    subscribe_timer.async_wait(std::bind(&unicorn_cluster_t::on_subscribe_timer, this, std::placeholders::_1));

    announce_timer.expires_from_now(boost::posix_time::seconds(config.retry_interval));
    announce_timer.async_wait(std::bind(&unicorn_cluster_t::on_announce_timer, this, std::placeholders::_1));
}

void
unicorn_cluster_t::announce() {
    const auto& actor = context.locate("locator");

    if(!actor) {
        COCAINE_LOG_ERROR(log, "unable to announce local endpoints: locator is not available");
        announce_timer.expires_from_now(boost::posix_time::seconds(config.retry_interval));
        announce_timer.async_wait(std::bind(&unicorn_cluster_t::on_announce_timer, this, std::placeholders::_1));
    }

    auto cur_endpoints = actor->location;

    COCAINE_LOG_DEBUG(log, "going to announce self");
    try {
        if(!endpoints.empty() && cur_endpoints != endpoints) {
            // Drop all ephemeral nodes, so create will succeed.
            // It's the easiest way to drop old announces as all clients will receive a notify on delete-create.
            auto lock = unicorn.synchronize();
            zk.reconnect();
        }
        endpoints.swap(cur_endpoints);
        unicorn->create(
            std::make_shared<on_announce>(this),
            config.path + '/' + locator.uuid(),
            endpoints,
            true,
            false
        );
        announce_timer.expires_from_now(boost::posix_time::seconds(config.check_interval));
        announce_timer.async_wait(std::bind(&unicorn_cluster_t::on_announce_timer, this, std::placeholders::_1));

    }
    catch(const std::system_error& e) {
        COCAINE_LOG_ERROR(log, "failure during announce(%i): %s", e.code().value(), e.what());
        announce_timer.expires_from_now(boost::posix_time::seconds(config.retry_interval));
        announce_timer.async_wait(std::bind(&unicorn_cluster_t::on_announce_timer, this, std::placeholders::_1));
        // Reconnect to prevent cases when is_unrecoverable returns false all the times, but we still get connection error.
        auto lock = unicorn.synchronize();
        zk.reconnect();
    }
}


void unicorn_cluster_t::on_announce_timer(const std::error_code& ec) {
    if(!ec) {
        announce();
    }
    //else timer was reset somewhere else
}

void unicorn_cluster_t::on_subscribe_timer(const std::error_code& ec) {
    if(!ec) {
        subscribe();
    }
    //else timer was reset somewhere else
}

void
unicorn_cluster_t::subscribe() {
    try {
        unicorn->children_subscribe(
            std::make_shared<on_list_update>(this),
            config.path
        );
        subscribe_timer.expires_from_now(boost::posix_time::seconds(config.check_interval));
        subscribe_timer.async_wait(std::bind(&unicorn_cluster_t::on_subscribe_timer, this, std::placeholders::_1));
        COCAINE_LOG_DEBUG(log, "subscribed for updates on %s", config.path.c_str());

    }
    catch(const std::system_error& e) {
        COCAINE_LOG_ERROR(log, "failure during subscription(%i): %s", e.code().value(), e.what());
        subscribe_timer.expires_from_now(boost::posix_time::seconds(config.retry_interval));
        subscribe_timer.async_wait(std::bind(&unicorn_cluster_t::on_subscribe_timer, this, std::placeholders::_1));
        // Reconnect to prevent cases when is_unrecoverable returns false all the times, but we still get connection error.
        auto lock = unicorn.synchronize();
        zk.reconnect();
    }
}
}}
