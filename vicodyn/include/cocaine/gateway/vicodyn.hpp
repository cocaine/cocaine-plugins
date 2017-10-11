#pragma once

#include "cocaine/idl/vicodyn.hpp"
#include "cocaine/vicodyn/forwards.hpp"
#include "cocaine/vicodyn/peer.hpp"

#include <cocaine/executor/asio.hpp>
#include <cocaine/api/cluster.hpp>
#include <cocaine/api/gateway.hpp>
#include <cocaine/api/service.hpp>
#include <cocaine/dynamic.hpp>
#include <cocaine/idl/context.hpp>
#include <cocaine/locked_ptr.hpp>
#include <cocaine/rpc/graph.hpp>
#include <cocaine/rpc/dispatch.hpp>


#include <asio/ip/tcp.hpp>

#include <map>

namespace cocaine {
namespace gateway {

class vicodyn_t : public api::gateway_t{
public:
    using endpoints_t = std::vector<asio::ip::tcp::endpoint>;

    vicodyn_t(context_t& context, const std::string& local_uuid, const std::string& name, const dynamic_t& args,
              const dynamic_t::object_t& locator_extra);
    ~vicodyn_t();

    struct proxy_description_t {
        proxy_description_t(std::unique_ptr<tcp_actor_t> _actor, vicodyn::proxy_t& _proxy);

        auto endpoints() const -> const std::vector<asio::ip::tcp::endpoint>&;

        auto protocol() const -> const io::graph_root_t&;

        auto version() const -> unsigned int;

        std::unique_ptr<tcp_actor_t> actor;
        vicodyn::proxy_t& proxy;
        std::vector<asio::ip::tcp::endpoint> cached_endpoints;
    };

    auto resolve_policy() const -> resolve_policy_t override {
        return resolve_policy_t::full;
    }

    auto resolve(const std::string& name) const -> service_description_t override;

    auto consume(const std::string& uuid,
                 const std::string& name,
                 unsigned int version,
                 const endpoints_t& endpoints,
                 const io::graph_root_t& protocol,
                 const dynamic_t::object_t& extra) -> void override;

    auto cleanup(const std::string& uuid, const std::string& name) -> void override;

    auto cleanup(const std::string& uuid) -> void override;

    auto total_count(const std::string& name) const -> size_t override;

private:
    using uuid_t = std::string;
    using app_name_t = std::string;
    using proxy_map_t = std::map<app_name_t, proxy_description_t>;

    auto create_wrapped_gateway() -> void;
    auto cleanup(proxy_map_t& map, proxy_map_t::iterator it, const std::string uuid) -> proxy_map_t::iterator;

    context_t& context;
    dynamic_t::object_t locator_extra;
    api::gateway_ptr wrapped_gateway;
    vicodyn::peers_t peers;
    dynamic_t args;
    std::string local_uuid;
    std::unique_ptr<logging::logger_t> logger;
    synchronized<proxy_map_t> mapping;
};

} // namespace gateway
} // namespace cocaine
