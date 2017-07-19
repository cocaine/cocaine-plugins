#pragma once

#include "cocaine/vicodyn/forwards.hpp"

#include <cocaine/forwards.hpp>
#include <cocaine/rpc/graph.hpp>

#include <asio/ip/tcp.hpp>

namespace cocaine {
namespace api {
namespace vicodyn {

class balancer_t {
public:

    typedef balancer_t category_type;

    using message_t = io::aux::decoded_message_t;

    virtual ~balancer_t() = default;

    balancer_t(context_t& context, asio::io_service& io_service, const std::string& service_name, const dynamic_t& conf);

    /// Process invocation inside pool. Peer selecting logic is usually applied before invocation.
    virtual
    auto choose_peer(const message_t& message, const cocaine::vicodyn::peers_t& peers) -> std::shared_ptr<cocaine::vicodyn::peer_t> = 0;

    virtual
    auto choose_intercept_peer(const cocaine::vicodyn::peers_t& peers) -> std::shared_ptr<cocaine::vicodyn::peer_t> = 0;

    virtual
    auto rebalance_peers(const cocaine::vicodyn::peers_t& peers) -> void = 0;
};

typedef std::shared_ptr<balancer_t> balancer_ptr;

auto balancer(context_t& context, asio::io_service& io_service, const std::string& balancer_name, const std::string& service_name)
        -> balancer_ptr;

} // namespace peer
} // namespace api
} // namespace cocaine
