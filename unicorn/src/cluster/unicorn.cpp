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

#include "cocaine/dynamic/constructors/endpoint.hpp"
#include "cocaine/dynamic/converters/endpoint.hpp"

#include <cocaine/context.hpp>
#include <cocaine/context/quote.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/format/exception.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/rpc/actor.hpp>
#include <cocaine/unicorn/value.hpp>

#include <asio/deadline_timer.hpp>

#include <blackhole/logger.hpp>

#include <boost/optional/optional.hpp>

#include <zookeeper/zookeeper.h>

namespace cocaine { namespace cluster {

namespace ph = std::placeholders;

typedef api::unicorn_t::response response;

unicorn_cluster_t::cfg_t::cfg_t(const dynamic_t& args) :
        path(args.as_object().at("path", "/cocaine/discovery").as_string()),
        retry_interval(args.as_object().at("retry_interval", 10u).as_uint())
{}

unicorn_cluster_t::timer_t::timer_t(unicorn_cluster_t& parent, std::function<void()> callback) :
    parent(parent),
    timer(parent.locator.asio()),
    callback(std::move(callback))
{}

auto unicorn_cluster_t::timer_t::defer(const boost::posix_time::time_duration& duration) -> void {
    timer.apply([&](asio::deadline_timer& timer){
        timer.expires_from_now(duration);
        timer.async_wait([&](const std::error_code& ec){
            if(!ec) {
                callback();
            }
        });
    });
}

auto unicorn_cluster_t::timer_t::defer_retry() -> void {
    defer(boost::posix_time::seconds(parent.config.retry_interval));
}

unicorn_cluster_t::announcer_t::announcer_t(unicorn_cluster_t& parent) :
        parent(parent),
        timer(parent, [=](){announce();}),
        path(parent.config.path + '/' + parent.locator.uuid())
{}

auto unicorn_cluster_t::announcer_t::announce() -> void {
    if(!set_and_check_endpoints()) {
        COCAINE_LOG_WARNING(parent.log, "unable to announce local endpoints: locator is not available");
        timer.defer_retry();
        return;
    }

    COCAINE_LOG_INFO(parent.log, "going to announce self");
    try {
        auto callback = std::bind(&announcer_t::on_set, this, ph::_1);
        scope = parent.unicorn->create(std::move(callback), path, endpoints, true, false);
    } catch (const std::system_error& e) {
        COCAINE_LOG_WARNING(parent.log, "could not announce self in unicorn - {}", error::to_string(e));
        timer.defer_retry();
    }
}

auto unicorn_cluster_t::announcer_t::set_and_check_endpoints() -> bool {
    auto quote = parent.context.locate("locator");

    if(!quote) {
        return false;
    }

    if(endpoints.empty()) {
        endpoints = quote->endpoints;
    }

    if(quote->endpoints != endpoints) {
        // TODO: fix this case, if ever we would need it
        // This can happen only if actor endpoints have changed
        // which is not likely.
        BOOST_ASSERT_MSG(false, "endpoints changed for locator service, can not continue, terminating" );
        std::terminate();
    }
    return true;
}

auto unicorn_cluster_t::announcer_t::on_set(std::future<response::create> future) -> void {
    try {
        future.get();
        COCAINE_LOG_INFO(parent.log, "announced self in unicorn");
    } catch (const std::system_error& e) {
        if(e.code() != error::node_exists) {
            COCAINE_LOG_ERROR(parent.log, "could not announce local services: {} ", error::to_string(e));
            timer.defer_retry();
            return;
        }
        COCAINE_LOG_INFO(parent.log, "announce checked");
    }
    auto cb = std::bind(&announcer_t::on_check, this, ph::_1);
    scope = parent.unicorn->subscribe(std::move(cb), std::move(path));
}

auto unicorn_cluster_t::announcer_t::on_check(std::future<response::subscribe> future) -> void {
    try {
        auto value = future.get();
        if (value.version() < 0) {
            COCAINE_LOG_ERROR(parent.log, "announce disappeared, retrying");
            timer.defer_retry();
        } else {
            COCAINE_LOG_INFO(parent.log, "fetched announce - {}", value.value());
        }
    } catch (const std::system_error& e) {
        COCAINE_LOG_ERROR(parent.log, "announce disappeared: {}, retrying", error::to_string(e));
        timer.defer_retry();
    }
}

unicorn_cluster_t::subscriber_t::subscriber_t(unicorn_cluster_t& parent) :
        parent(parent),
        timer(parent, [=](){subscribe();})
{}

auto unicorn_cluster_t::subscriber_t::subscribe() -> void {
    auto cb = std::bind(&unicorn_cluster_t::subscriber_t::on_children, this, ph::_1);
    const auto& path = parent.config.path;
    children_scope = parent.unicorn->children_subscribe(std::move(cb), path);
    COCAINE_LOG_INFO(parent.log, "subscribed for updates on path {} (may be prefixed)", path);
}

auto unicorn_cluster_t::subscriber_t::on_children(std::future<response::children_subscribe> future) -> void {
    try {
        update_state(std::get<1>(future.get()));
    } catch (const std::system_error& e) {
        COCAINE_LOG_WARNING(parent.log, "failure during subscription: {}, resubscribing", error::to_string(e));
        timer.defer_retry();
    }
}

auto unicorn_cluster_t::subscriber_t::update_state(std::vector<std::string> nodes) -> void {
    COCAINE_LOG_INFO(parent.log, "received uuid list from zookeeper, got {} uuids", nodes.size());
    std::set<std::string> nodes_set(nodes.begin(), nodes.end());
    subscriptions.apply([&](subscriptions_t& subscriptions) {
        for(auto it = subscriptions.begin(); it != subscriptions.end();) {
            if(!nodes_set.count(it->first)) {
                auto uuid = it->first;
                parent.locator.drop_node(uuid);
                it = subscriptions.erase(it);
                COCAINE_LOG_INFO(parent.log, "dropped node {}", uuid);
            } else {
                it++;
            }
        }
        for(const auto& node: nodes) {
            if(node == parent.locator.uuid()) {
                continue;
            }
            auto& subscription = subscriptions[node];
            if(!subscription.endpoints.empty()) {
                COCAINE_LOG_INFO(parent.log, "relinking node {}", node);
                parent.locator.link_node(node, subscription.endpoints);
            } else {
                COCAINE_LOG_INFO(parent.log, "subscribing on a new node {}", node);
                auto cb = std::bind(&unicorn_cluster_t::subscriber_t::on_node, this, node, ph::_1);
                subscription.scope = parent.unicorn->subscribe(std::move(cb), parent.config.path + '/' + node);
            }
        }
        COCAINE_LOG_INFO(parent.log, "endpoint map now contains {} uuids", subscriptions.size());
    });
}

auto unicorn_cluster_t::subscriber_t::on_node(std::string uuid, std::future<response::subscribe> future) -> void {
    subscriptions.apply([&](subscriptions_t& subscriptions) {
        auto terminate = [&](const std::string& reason) {
            COCAINE_LOG_WARNING(parent.log, "node {} subscription failed - {}", uuid, reason);
            parent.locator.drop_node(uuid);
            subscriptions.erase(uuid);
        };

        try {
            auto result = future.get();
            if(!result.exists()) {
                terminate("node was removed");
            }
            auto endpoints = result.value().to<std::vector<asio::ip::tcp::endpoint>>();
            if(endpoints.empty()) {
                terminate("node has empty endpoints");
            }
            auto& subscription = subscriptions[uuid];
            if(!subscription.endpoints.empty() && subscription.endpoints != endpoints) {
                terminate("received endpoints different from stored");
            }
            subscription.endpoints = std::move(endpoints);
            parent.locator.link_node(uuid, subscription.endpoints);
            COCAINE_LOG_INFO(parent.log, "linked node {}", uuid);
        } catch(const std::exception& e){
            COCAINE_LOG_WARNING(parent.log, "failure during subscription on node {} - {}", uuid, e);
            subscriptions[uuid].endpoints.clear();
            timer.defer_retry();
        }
    });
}

unicorn_cluster_t::unicorn_cluster_t(
    cocaine::context_t & _context,
    cocaine::api::cluster_t::interface & _locator,
    mode_t mode,
    const std::string& name,
    const cocaine::dynamic_t& args
):
    cluster_t(_context, _locator, mode, name, args),
    log(_context.log("unicorn/cluster")),
    config(args),
    context(_context),
    locator(_locator),
    unicorn(api::unicorn(_context, args.as_object().at("backend").as_string())),
    announcer(*this),
    subscriber(*this)
{
    if(mode == mode_t::full) {
        subscriber.subscribe();
    }
    announcer.announce();
}

}}
