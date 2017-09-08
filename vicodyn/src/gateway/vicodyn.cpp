#include "cocaine/gateway/vicodyn.hpp"

#include "../../node/include/cocaine/idl/node.hpp"

#include "cocaine/vicodyn/proxy.hpp"

#include <cocaine/context.hpp>
#include <cocaine/context/quote.hpp>
#include <cocaine/context/signal.hpp>
#include <cocaine/dynamic.hpp>
#include <cocaine/format/endpoint.hpp>
#include <cocaine/format/vector.hpp>
#include <cocaine/idl/context.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/memory.hpp>
#include <cocaine/repository.hpp>
#include <cocaine/repository/gateway.hpp>
#include <cocaine/rpc/actor.hpp>
#include <cocaine/traits/endpoint.hpp>
#include <cocaine/traits/vector.hpp>

#include <blackhole/logger.hpp>


namespace cocaine {
namespace gateway {

namespace ph = std::placeholders;

vicodyn_t::proxy_description_t::proxy_description_t(std::unique_ptr<tcp_actor_t> _actor, vicodyn::proxy_t& _proxy) :
        actor(std::move(_actor)),
        proxy(_proxy)
{}

auto vicodyn_t::proxy_description_t::endpoints() const -> std::vector<asio::ip::tcp::endpoint> {
    return actor->endpoints();
}

auto vicodyn_t::proxy_description_t::protocol() const -> io::graph_root_t {
    return proxy.root();
}

auto vicodyn_t::proxy_description_t::version() const -> unsigned int {
    return proxy.version();
}

auto vicodyn_t::create_wrapped_gateway() -> void {
    auto wrapped_conf = args.as_object().at("wrapped", dynamic_t::empty_object).as_object();
    auto wrapped_name = wrapped_conf.at("type", "adhoc").as_string();
    auto wrapped_args = wrapped_conf.at("args", dynamic_t::empty_object);
    wrapped_gateway = context.repository().get<gateway_t>(wrapped_name, context, local_uuid, "vicodyn/wrapped", wrapped_args);
}

vicodyn_t::vicodyn_t(context_t& _context, const std::string& _local_uuid, const std::string& name, const dynamic_t& args) :
    gateway_t(_context, _local_uuid, name, args),
    context(_context),
    wrapped_gateway(),
    peers(context),
    args(args),
    local_uuid(_local_uuid),
    logger(context.log(format("gateway/{}", name)))
{
    create_wrapped_gateway();
    COCAINE_LOG_DEBUG(logger, "creatied vicodyn service for  {} with local uuid {}", name, local_uuid);
}

vicodyn_t::~vicodyn_t() {
    COCAINE_LOG_DEBUG(logger, "shutting down vicodyn gateway");
    for (auto& proxy_pair: mapping.unsafe()) {
        proxy_pair.second.actor->terminate();
        COCAINE_LOG_INFO(logger, "stopped {} virtual service", proxy_pair.first);
    }
    mapping.unsafe().clear();
}

auto vicodyn_t::resolve(const std::string& name) const -> service_description_t {
    static const io::graph_root_t app_root = io::traverse<io::app_tag>().get();
    const auto local = context.locate(name);
    if(local && local->prototype->root() != app_root) {
        COCAINE_LOG_DEBUG(logger, "providing local non-app service");
        auto version = static_cast<unsigned int>(local->prototype->version());
        return service_description_t {local->endpoints, local->prototype->root(), version};
    }

    return mapping.apply([&](const proxy_map_t& mapping){
        auto it = mapping.find(name);
        if(it == mapping.end()) {
            COCAINE_LOG_INFO(logger, "resolving non-app service via wrapped gateway");
            return wrapped_gateway->resolve(name);
        }
        COCAINE_LOG_INFO(logger, "providing virtual app service {} on {}", name, it->second.endpoints());
        return service_description_t{it->second.endpoints(), it->second.protocol(), it->second.version()};
    });
}

auto vicodyn_t::consume(const std::string& uuid,
                        const std::string& name,
                        unsigned int version,
                        const std::vector<asio::ip::tcp::endpoint>& endpoints,
                        const io::graph_root_t& protocol) -> void
{
    static const io::graph_root_t node_protocol = io::traverse<io::node_tag>().get();
    static const io::graph_root_t app_protocol = io::traverse<io::app_tag>().get();
    wrapped_gateway->consume(uuid, name, version, endpoints, protocol);
    mapping.apply([&](proxy_map_t& mapping){
        if(protocol == node_protocol) {
            peers.register_peer(uuid, endpoints);
            COCAINE_LOG_INFO(logger, "registered node service {} with uuid {}", name, uuid);
        } else if (protocol == app_protocol) {
            peers.register_app(uuid, name);
            auto it = mapping.find(name);
            if(it == mapping.end()) {
                auto proxy = std::make_unique<vicodyn::proxy_t>(context, peers, "virtual::" + name, args);
                auto& proxy_ref = *proxy;
                auto actor = std::make_unique<tcp_actor_t>(context, std::move(proxy));
                actor->run();
                mapping.emplace(name, proxy_description_t(std::move(actor), proxy_ref));
                COCAINE_LOG_INFO(logger, "created new virtual service {}", name);
            } else {
                COCAINE_LOG_INFO(logger, "registered app {} on uuid {} in existing virtual service", name, uuid);
            }
        } else {
            COCAINE_LOG_INFO(logger, "delegating unknown protocol service {} to wrapped gateway", name);
            return;
        }
    });
    COCAINE_LOG_INFO(logger, "exposed {} service to vicodyn", name);
}

auto vicodyn_t::cleanup(const std::string& uuid, const std::string& name) -> void {
    if(name == "node") {
        peers.erase_peer(uuid);
        COCAINE_LOG_INFO(logger, "dropped node service on {}", uuid);
    } else {
        peers.erase_app(uuid, name);
        mapping.apply([&](proxy_map_t& mapping){
            auto it = mapping.find(name);
            if(it != mapping.end()) {
                if(it->second.proxy.empty()) {
                    it->second.actor->terminate();
                    mapping.erase(it);
                }
            }
        });
    }
    wrapped_gateway->cleanup(uuid, name);
    COCAINE_LOG_INFO(logger, "dropped service {} on {}", name, uuid);
}

auto vicodyn_t::cleanup(const std::string& uuid) -> void {
    peers.erase(uuid);
    mapping.apply([&](proxy_map_t& mapping){
        for(auto it = mapping.begin(); it != mapping.end();) {
            if(it->second.proxy.empty()) {
                it->second.actor->terminate();
                mapping.erase(it);
            } else {
                it++;
            }
        }
    });
    wrapped_gateway->cleanup(uuid);
    COCAINE_LOG_INFO(logger, "fully dropped uuid {}", uuid);
}

auto vicodyn_t::total_count(const std::string& name) const -> size_t {
    return mapping.apply([&](const proxy_map_t& mapping) -> size_t {
        auto it = mapping.find(name);
        if(it == mapping.end()) {
            return wrapped_gateway->total_count(name);
        }
        return it->second.proxy.size();
    });
}

} // namespace gateway
} // namespace cocaine
