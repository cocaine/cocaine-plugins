#include "cocaine/gateway/vicodyn.hpp"

#include "../../node/include/cocaine/idl/node.hpp"

#include "cocaine/vicodyn/proxy.hpp"

#include <cocaine/context.hpp>
#include <cocaine/context/quote.hpp>
#include <cocaine/context/signal.hpp>
#include <cocaine/dynamic.hpp>
#include <cocaine/idl/context.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/memory.hpp>
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

vicodyn_t::vicodyn_t(context_t& _context, const std::string& _local_uuid, const std::string& name, const dynamic_t& args) :
    gateway_t(_context, _local_uuid, name, args),
    context(_context),
    args(args),
    local_uuid(_local_uuid),
    logger(context.log(name))
{
    COCAINE_LOG_DEBUG(logger, "creating vicodyn service for  {} with local uuid {}", name, local_uuid);
}

vicodyn_t::~vicodyn_t() {
    COCAINE_LOG_DEBUG(logger, "shutting down vicodyn gateway");
    for (auto& proxy_pair: mapping.unsafe().proxy_map) {
        proxy_pair.second.actor->terminate();
        COCAINE_LOG_INFO(logger, "stopped {} virtual service", proxy_pair.first);
    }
    mapping.unsafe().proxy_map.clear();
}

auto vicodyn_t::resolve(const std::string& name) const -> service_description_t {
    static const io::graph_root_t app_root = io::traverse<io::app_tag>().get();
    const auto local = context.locate(name);
    if(local && local->prototype->root() != app_root) {
        COCAINE_LOG_DEBUG(logger, "providing local non application service");
        auto version = static_cast<unsigned int>(local->prototype->version());
        return service_description_t {local->endpoints, local->prototype->root(), version};
    }

    return mapping.apply([&](const mapping_t& mapping){
        auto it = mapping.proxy_map.find(name);
        if(it == mapping.proxy_map.end()) {
            throw error_t(error::service_not_available, "service {} not found in vicodyn", name);
        }
        return service_description_t{it->second.endpoints(), it->second.protocol(), it->second.version()};
    });
}

auto vicodyn_t::consume(const std::string& uuid,
                        const std::string& name,
                        unsigned int,
                        const std::vector<asio::ip::tcp::endpoint>& endpoints,
                        const io::graph_root_t& protocol) -> void
{
    COCAINE_LOG_DEBUG(logger, "exposing {} service to vicodyn", name);
    static const io::graph_root_t node_protocol = io::traverse<io::node_tag>().get();
    static const io::graph_root_t app_protocol = io::traverse<io::app_tag>().get();
    mapping.apply([&](mapping_t& mapping){
        if(protocol == node_protocol) {
            COCAINE_LOG_INFO(logger, "registering node service {} with uuid {}", name, uuid);
            mapping.node_map[uuid] = endpoints;
            for(auto& proxy_pair: mapping.proxy_map) {
                proxy_pair.second.proxy.register_node(uuid, endpoints);
            }
        } else if (protocol == app_protocol) {
            auto it = mapping.proxy_map.find(name);
            if(it != mapping.proxy_map.end()) {
                it->second.proxy.register_real(uuid);
                COCAINE_LOG_INFO(logger, "registered real in existing {} virtual service", name);
            } else {
                auto proxy = std::make_unique<vicodyn::proxy_t>(context, "virtual::" + name, args);
                proxy->register_real(uuid);
                auto node_it = mapping.node_map.find(uuid);
                if(node_it != mapping.node_map.end()) {
                    proxy->register_node(uuid, node_it->second);
                }
                auto& proxy_ref = *proxy;
                auto actor = std::make_unique<tcp_actor_t>(context, std::move(proxy));
                actor->run();
                mapping.proxy_map.emplace(name, proxy_description_t(std::move(actor), proxy_ref));
                COCAINE_LOG_INFO(logger, "created new virtual service {}", name);
            }
        } else {
            COCAINE_LOG_INFO(logger, "unsupported protocol service {}", name);
            return;
        }
    });
}

auto vicodyn_t::cleanup(const std::string& uuid, const std::string& name) -> void {
    mapping.apply([&](mapping_t& mapping){
        if(name == "node") {
            for(auto& proxy_pair: mapping.proxy_map) {
                proxy_pair.second.proxy.deregister_node(uuid);
            }
        } else {
            auto it = mapping.proxy_map.find(name);
            if(it != mapping.proxy_map.end()) {
                it->second.proxy.deregister_real(uuid);
            }
        }
    });
}

auto vicodyn_t::cleanup(const std::string& uuid) -> void {
    mapping.apply([&](mapping_t& mapping){
        for(auto& proxy_pair: mapping.proxy_map) {
            proxy_pair.second.proxy.deregister_node(uuid);
            proxy_pair.second.proxy.deregister_real(uuid);
        }
    });
}

auto vicodyn_t::total_count(const std::string& name) const -> size_t {
    return mapping.apply([&](const mapping_t& mapping) -> size_t {
        auto it = mapping.proxy_map.find(name);
        if(it == mapping.proxy_map.end()) {
            return 0u;
        }
        return it->second.proxy.size();
    });
}

} // namespace gateway
} // namespace cocaine
