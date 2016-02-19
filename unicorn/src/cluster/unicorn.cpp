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

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

#include <cocaine/rpc/actor.hpp>

#include <cocaine/unicorn/value.hpp>

#include <blackhole/logger.hpp>

#include <zookeeper/zookeeper.h>

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
        auto ep_pair = from.as_array();
        if (!ep_pair[0].is_string() || !ep_pair[1].is_uint()) {
            throw std::runtime_error("invalid dynamic value for endpoint deserialization");
        }
        std::string host = ep_pair[0].to<std::string>();
        unsigned short int port = ep_pair[1].to<unsigned short int>();
        asio::ip::tcp::endpoint result(asio::ip::address::from_string(host), port);
        return result;
    }

    static inline
    bool
    convertible(const dynamic_t& from) {
        return from.is_array() &&
               from.as_array().size() == 2 &&
               from.as_array()[0].is_string() &&
               (from.as_array()[1].is_uint() || from.as_array()[1].is_int());
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

} // namespace cocaine

namespace cocaine { namespace cluster {

struct unicorn_cluster_t::on_announce:
    public unicorn::writable_adapter_base_t<api::unicorn_t::response::create>,
    public std::enable_shared_from_this<on_announce>
{
    on_announce(unicorn_cluster_t* _parent);

    virtual void
    write(api::unicorn_t::response::create&& result);

    virtual void
    abort(const std::error_code& ec, const std::string& reason);

    unicorn_cluster_t* parent;
};

struct unicorn_cluster_t::on_update:
    public unicorn::writable_adapter_base_t<api::unicorn_t::response::subscribe>,
    public std::enable_shared_from_this<on_update>
{
    on_update(unicorn_cluster_t* _parent);

    virtual void
    write(api::unicorn_t::response::subscribe&& result);

    using unicorn::writable_adapter_base_t<api::unicorn_t::response::subscribe>::abort;

    virtual void
    abort(const std::error_code& ec, const std::string& reason);

    unicorn_cluster_t* parent;
};

struct unicorn_cluster_t::on_fetch :
    public unicorn::writable_adapter_base_t<api::unicorn_t::response::get>
{
    on_fetch(std::string uuid, unicorn_cluster_t* _parent);

    virtual void
    write(api::unicorn_t::response::get&& result);

    virtual void
    abort(const std::error_code& ec, const std::string& reason);

    std::string uuid;
    unicorn_cluster_t* parent;
};

struct unicorn_cluster_t::on_list_update :
    public unicorn::writable_adapter_base_t<api::unicorn_t::response::children_subscribe>
{
    on_list_update(unicorn_cluster_t* _parent);

    virtual void
    write(api::unicorn_t::response::children_subscribe&& result);

    virtual void
    abort(const std::error_code& ec, const std::string& reason);

    unicorn_cluster_t* parent;
};

unicorn_cluster_t::cfg_t::cfg_t(const dynamic_t& args) :
    path(args.as_object().at("path", "/cocaine/discovery").as_string()),
    retry_interval(args.as_object().at("retry_interval", 1u).as_uint()),
    check_interval(args.as_object().at("check_interval", 60u).as_uint())
{}

unicorn_cluster_t::on_announce::on_announce(unicorn_cluster_t* _parent) :
    parent(_parent)
{}

void
unicorn_cluster_t::on_announce::write(api::unicorn_t::response::create&& /*result*/) {
    COCAINE_LOG_INFO(parent->log, "announced self in unicorn");
    parent->subscribe_scope = parent->unicorn->subscribe(std::make_shared<on_update>(parent), parent->config.path + '/' + parent->locator.uuid());
}

void
unicorn_cluster_t::on_announce::abort(const std::error_code& rc, const std::string& reason) {
    //Ok for us.
    if(rc.value() == ZNODEEXISTS) {
        COCAINE_LOG_INFO(parent->log, "announce checked");
        return;
    }
    COCAINE_LOG_ERROR(parent->log, "could not announce local services({}): {} - {}", rc.value(), rc.message(), reason);
    parent->announce_timer.expires_from_now(boost::posix_time::seconds(parent->config.retry_interval));
    parent->announce_timer.async_wait(std::bind(&unicorn_cluster_t::on_announce_timer, parent, std::placeholders::_1));
}

unicorn_cluster_t::on_update::on_update(unicorn_cluster_t* _parent) :
    parent(_parent)
{}

void
unicorn_cluster_t::on_update::write(api::unicorn_t::response::subscribe&& result) {
    //Node disappeared
    if(result.get_version() < 0) {
        COCAINE_LOG_ERROR(parent->log, "announce dissappeared, retrying");
        parent->announce();
    }
}

void
unicorn_cluster_t::on_update::abort(const std::error_code& rc, const std::string& reason) {
    COCAINE_LOG_ERROR(parent->log, "announce dissappeared({}): {} - {}, retrying", rc.value(), rc.message(), reason);
    parent->announce();
}

unicorn_cluster_t::on_fetch::on_fetch(std::string _uuid, unicorn_cluster_t* _parent) :
    uuid(std::move(_uuid)),
    parent(_parent)
{}

void
unicorn_cluster_t::on_fetch::write(api::unicorn_t::response::get&& result) {
    if(!result.get_value().convertible_to<std::vector<asio::ip::tcp::endpoint>>()) {
        COCAINE_LOG_WARNING(parent->log, "invalid value for announce: {}",
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
unicorn_cluster_t::on_fetch::abort(const std::error_code& rc, const std::string& reason) {
    COCAINE_LOG_WARNING(parent->log, "error during fetch({}): {} - {}", rc.value(), rc.message(), reason);
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
unicorn_cluster_t::on_list_update::write(api::unicorn_t::response::children_subscribe&& result) {
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
        parent->get_scope = parent->unicorn->get(
            std::make_shared<on_fetch>(to_add[i], parent),
            parent->config.path + '/' + to_add[i]
        );
    }
}

void
unicorn_cluster_t::on_list_update::abort(const std::error_code& rc, const std::string& reason) {
    COCAINE_LOG_WARNING(parent->log, "failure during subscription({}): {} - {}, resubscribing", rc.value(), rc.message(), reason);
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
    unicorn(api::unicorn(_context, args.as_object().at("backend").as_string()))
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

    auto cur_endpoints = actor->endpoints();

    COCAINE_LOG_INFO(log, "going to announce self");
    if(!endpoints.empty() && cur_endpoints != endpoints) {
        // TODO: fix this case, if ever we would need it
        // This can happen only if actor endpoints have changed
        // which is not likely.
        BOOST_ASSERT_MSG(false, "endpoints changed for locator sercice, can not comtimue, terminating" );
        std::terminate();
    } else {
        endpoints.swap(cur_endpoints);
        create_scope = unicorn->create(
            std::make_shared<on_announce>(this),
            config.path + '/' + locator.uuid(),
            endpoints,
            true,
            false
        );
        announce_timer.expires_from_now(boost::posix_time::seconds(config.check_interval));
        announce_timer.async_wait(std::bind(&unicorn_cluster_t::on_announce_timer, this, std::placeholders::_1));
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
    children_subscribe_scope = unicorn->children_subscribe(
        std::make_shared<on_list_update>(this),
        config.path
    );
    subscribe_timer.expires_from_now(boost::posix_time::seconds(config.check_interval));
    subscribe_timer.async_wait(std::bind(&unicorn_cluster_t::on_subscribe_timer, this, std::placeholders::_1));
    COCAINE_LOG_INFO(log, "subscribed for updates on path {} (may be prefixed)", config.path);
}

}}
