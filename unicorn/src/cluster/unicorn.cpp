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
#include <cocaine/errors.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/rpc/actor.hpp>
#include <cocaine/unicorn/value.hpp>

#include <blackhole/logger.hpp>

#include <boost/optional/optional.hpp>

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

namespace ph = std::placeholders;

typedef api::unicorn_t::response response;


unicorn_cluster_t::cfg_t::cfg_t(const dynamic_t& args) :
    path(args.as_object().at("path", "/cocaine/discovery").as_string()),
    retry_interval(args.as_object().at("retry_interval", 1u).as_uint()),
    check_interval(args.as_object().at("check_interval", 60u).as_uint())
{}

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
    subscribe();
    announce();
}

void
unicorn_cluster_t::announce() {
    const auto& actor = context.locate("locator");

    if(!actor) {
        COCAINE_LOG_ERROR(log, "unable to announce local endpoints: locator is not available");
        announce_timer.expires_from_now(boost::posix_time::seconds(config.retry_interval));
        announce_timer.async_wait(std::bind(&unicorn_cluster_t::on_announce_timer, this, std::placeholders::_1));
        return;
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
            std::bind(&unicorn_cluster_t::on_announce_set, this, ph::_1),
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

void unicorn_cluster_t::on_announce_set(std::future<api::unicorn_t::response::create> future) {
    try {
        future.get();
        COCAINE_LOG_INFO(log, "announced self in unicorn");
        auto cb = std::bind(&unicorn_cluster_t::on_announce_checked, this, ph::_1);
        auto path = config.path + '/' + locator.uuid();
        subscribe_scope = unicorn->subscribe(std::move(cb), path);
    } catch (const std::system_error& e) {
        if(e.code().value() == ZNODEEXISTS) {
            COCAINE_LOG_INFO(log, "announce checked");
            return;
        }
        COCAINE_LOG_ERROR(log, "could not announce local services: {} ", error::to_string(e));
        announce_timer.expires_from_now(boost::posix_time::seconds(config.retry_interval));
        announce_timer.async_wait(std::bind(&unicorn_cluster_t::on_announce_timer, this, std::placeholders::_1));
    }
};

void unicorn_cluster_t::on_announce_checked (std::future<api::unicorn_t::response::subscribe> future) {
    try {
        if (future.get().get_version() < 0) {
            COCAINE_LOG_ERROR(log, "announce dissappeared, retrying");
            announce();
        }
    } catch (const std::system_error& e) {
        COCAINE_LOG_ERROR(log, "announce dissappeared: {} , retrying", error::to_string(e));
        announce();
    }
};

void unicorn_cluster_t::on_node_list_change(std::future<api::unicorn_t::response::children_subscribe> new_list) {
    try {
        auto result = new_list.get();
        auto nodes = std::get<1>(result);
        COCAINE_LOG_INFO(log, "received uuid list from zookeeper, got {} uuids", nodes.size());
        std::sort(nodes.begin(), nodes.end());
        std::vector<std::string> to_delete, to_add;
        std::set<std::string> uuids;
        registered_locators.apply([&](locator_endpoints_t& endpoint_map) {
            for (const auto& uuid_to_endpoints : endpoint_map) {
                uuids.insert(uuid_to_endpoints.first);
            }
            std::set_difference(nodes.begin(),
                                nodes.end(),
                                uuids.begin(),
                                uuids.end(),
                                std::back_inserter(to_add));

            std::set_difference(uuids.begin(),
                                uuids.end(),
                                nodes.begin(),
                                nodes.end(),
                                std::back_inserter(to_delete));
            COCAINE_LOG_INFO(log,
                             "{} nodes to drop and {} nodes to add (may include this)",
                             to_delete.size(),
                             to_add.size());
            for (size_t i = 0; i < to_delete.size(); i++) {
                locator.drop_node(to_delete[i]);
                endpoint_map.erase(to_delete[i]);
            }
            for (size_t i = 0; i < to_add.size(); i++) {
                endpoint_map[to_add[i]] = {};
                auto callback = std::bind(&unicorn_cluster_t::on_node_fetch, this, to_add[i], std::placeholders::_1);
                get_scope = unicorn->get(std::move(callback), config.path + '/' + to_add[i]);
            }
            COCAINE_LOG_INFO(log, "endpoint map now contains {} uuids", endpoint_map.size());
        });
    } catch (const std::system_error& e) {
        COCAINE_LOG_WARNING(log, "failure during subscription: {} , resubscribing", error::to_string(e));
        subscribe_timer.expires_from_now(boost::posix_time::seconds(config.retry_interval));
        subscribe_timer.async_wait(std::bind(&unicorn_cluster_t::on_subscribe_timer, this, std::placeholders::_1));
    }
}

void unicorn_cluster_t::on_node_fetch(const std::string& uuid, std::future<api::unicorn_t::response::get> node_endpoints) {
    try {
        auto result = node_endpoints.get();
        COCAINE_LOG_INFO(log, "fetched {} node's endpoint from zookeeper", uuid);
        std::vector<asio::ip::tcp::endpoint> fetched_endpoints;
        if (result.get_value().convertible_to<std::vector<asio::ip::tcp::endpoint>>()) {
            fetched_endpoints = result.get_value().to<std::vector<asio::ip::tcp::endpoint>>();
        }
        if (fetched_endpoints.empty()) {
            COCAINE_LOG_WARNING(log, "invalid value for announce: {}",
                                boost::lexical_cast<std::string>(result.get_value()).c_str()
            );
            registered_locators.apply([&](locator_endpoints_t& endpoint_map) {
                endpoint_map.erase(uuid);
            });
        } else {
            registered_locators.apply([&](locator_endpoints_t& endpoint_map) {
                auto& cur_endpoints = endpoint_map[uuid];
                if (cur_endpoints.empty()) {
                    cur_endpoints = fetched_endpoints;
                    if (uuid != locator.uuid()) {
                        COCAINE_LOG_INFO(log, "linking {} node", uuid);
                        locator.link_node(uuid, fetched_endpoints);
                    }
                } else {
                    COCAINE_LOG_WARNING(log, "received announce on existing uuid");
                }
            });
        }
    } catch (const std::system_error& e) {
        COCAINE_LOG_WARNING(log, "error during fetch: {} ", error::to_string(e));
        registered_locators.apply([&](locator_endpoints_t& endpoint_map) {
            auto it = endpoint_map.find(uuid);
            if (it != endpoint_map.end()) {
                if (!it->second.empty()) {
                    locator.drop_node(uuid);
                }
                endpoint_map.erase(it);
            }
        });
    }
}

void
unicorn_cluster_t::subscribe() {
    children_subscribe_scope = unicorn->children_subscribe(
        std::bind(&unicorn_cluster_t::on_node_list_change, this, std::placeholders::_1),
        config.path
    );
    subscribe_timer.expires_from_now(boost::posix_time::seconds(config.check_interval));
    subscribe_timer.async_wait(std::bind(&unicorn_cluster_t::on_subscribe_timer, this, std::placeholders::_1));
    COCAINE_LOG_INFO(log, "subscribed for updates on path {} (may be prefixed)", config.path);
}

}}
