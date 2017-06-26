#pragma once

#include "cocaine/api/vicodyn/balancer.hpp"

#include <cocaine/dynamic.hpp>

namespace cocaine {
namespace vicodyn {
namespace balancer {

class simple_t: public api::vicodyn::balancer_t {
public:
    simple_t(context_t& context, asio::io_service& io_service, const std::string& service_name, const dynamic_t& conf);

    /// Process invocation inside pool. Peer selecting logic is usually applied before invocation.
    auto choose_peer(const message_t& message, const peers_t& peers) -> std::shared_ptr<peer_t> override;

    auto choose_intercept_peer(const peers_t& peers) -> std::shared_ptr<peer_t> override;

    auto rebalance_peers(const cocaine::vicodyn::peers_t& peers) -> void override;

private:
    auto choose_peer(const peers_t& peers) -> std::shared_ptr<peer_t>;

    dynamic_t conf;
    std::unique_ptr<logging::logger_t> logger;
};

} // namespace balancer
} // namespace vicodyn
} // namespace cocaine
