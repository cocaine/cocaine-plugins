#include "cocaine/gateway/vicodyn.hpp"

#include "cocaine/vicodyn/debug.hpp"
#include "cocaine/vicodyn/proxy.hpp"

#include <cocaine/context.hpp>
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
    local_uuid(_local_uuid),
    logger(context.log(name))
{
    VICODYN_REGISTER_LOGGER(context.log("vicodyn_debug_logger"));
    COCAINE_LOG_DEBUG(logger, "creating vicodyn service for  {} with local uuid {}", name, local_uuid);
}

vicodyn_t::~vicodyn_t() {
    COCAINE_LOG_DEBUG(logger, "shutting down vicodyn gateway");
    for (auto& proxy_pair: proxy_map.unsafe()) {
        proxy_pair.second.actor->terminate();
        COCAINE_LOG_INFO(logger, "stopped {} virtual service", proxy_pair.first);
    }
    proxy_map.unsafe().clear();
}

auto vicodyn_t::resolve(const std::string& name) const -> service_description_t {
    return proxy_map.apply([&](const proxy_map_t& proxies){
        auto it = proxies.find(name);
        if(it == proxies.end()) {
            throw error_t(error::service_not_available, "service {} not found in vicodyn", name);
        }
        return service_description_t{it->second.endpoints(), it->second.protocol(), it->second.version()};
    });
}

auto vicodyn_t::consume(const std::string& uuid,
                        const std::string& name,
                        unsigned int version,
                        const std::vector<asio::ip::tcp::endpoint>& endpoints,
                        const io::graph_root_t& protocol) -> void
{
    COCAINE_LOG_DEBUG(logger, "exposing {} service to vicodyn", name);
    proxy_map.apply([&](proxy_map_t& proxies){
        // lookup it in our table
        auto it = proxies.find(name);
        if(it != proxies.end()) {
            it->second.proxy.register_real(uuid, endpoints, uuid.empty());
            COCAINE_LOG_INFO(logger, "registered real in existing {} virtual service", name);
        } else {
            auto asio = std::make_shared<asio::io_service>();
            auto proxy = std::make_unique<vicodyn::proxy_t>(context,
                                                            asio,
                                                            "virtual::" + name,
                                                            dynamic_t(),
                                                            version,
                                                            protocol);
            auto& proxy_ref = *proxy;
            proxy->register_real(uuid, endpoints, uuid.empty());
            auto actor = std::make_unique<tcp_actor_t>(context, std::move(proxy));
            actor->run();
            proxies.emplace(name, proxy_description_t(std::move(actor), proxy_ref));
            COCAINE_LOG_INFO(logger, "created new virtual service {}", name);
        }
    });
}

auto vicodyn_t::cleanup(proxy_map_t& proxies, proxy_map_t::iterator it, const std::string uuid)
    -> proxy_map_t::iterator
{
    it->second.proxy.deregister_real(uuid);
    COCAINE_LOG_INFO(logger, "removed {} remote for service {}", uuid, it->first);
    if(it->second.proxy.empty()) {
        it->second.actor->terminate();
        auto name = it->first;
        it = proxies.erase(it);
        COCAINE_LOG_INFO(logger, "proxy for {} was shut down: no remotes left", name);
    } else {
        it++;
    }
    return it;
}

auto vicodyn_t::cleanup(const std::string& uuid, const std::string& name) -> void {
    proxy_map.apply([&](proxy_map_t& proxies){
        auto it = proxies.find(name);
        if(it == proxies.end()) {
            COCAINE_LOG_ERROR(logger, "remote service {} from {} scheduled for removal not found in vicodyn", name, uuid);
            return;
        }
        cleanup(proxies, it, uuid);
    });
}

auto vicodyn_t::cleanup(const std::string& uuid) -> void {
    proxy_map.apply([&](proxy_map_t& proxies) {
        for (auto it = proxies.begin(); it != proxies.end();) {
            it = cleanup(proxies, it, uuid);
        }
    });
}

auto vicodyn_t::total_count(const std::string& name) const -> size_t {
    return proxy_map.apply([&](const proxy_map_t& proxies) -> size_t {
        auto it = proxies.find(name);
        if(it == proxies.end()) {
            return 0ul;
        }
        return it->second.proxy.size();
    });
}

} // namespace gateway
} // namespace cocaine
