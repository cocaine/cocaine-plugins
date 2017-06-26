#pragma once

#include "cocaine/api/vicodyn/balancer.hpp"
#include "cocaine/vicodyn/peer.hpp"

#include <cocaine/locked_ptr.hpp>

namespace cocaine {
namespace vicodyn {

class pool_t {
public:
    using message_t = io::aux::decoded_message_t;

    pool_t(context_t& _context, asio::io_service& _io_loop, const std::string& service_name, const dynamic_t& conf);

    ~pool_t();

    auto invoke(const message_t& incoming, const io::graph_node_t& protocol, stream_ptr_t backward_stream) -> stream_ptr_t;

    auto register_real(std::string uuid, std::vector<asio::ip::tcp::endpoint> ep, bool) -> void;

    auto deregister_real(const std::string& uuid) -> void;

    auto size() -> size_t;

private:
    auto rebalance_peers() -> void;
    auto on_peer_state(peer_t::state_result_t state_result) -> void;

    std::string service_name;
    std::shared_ptr<dispatch<io::context_tag>> signal_dispatcher;
    size_t pool_size;
    size_t retry_count;
    std::chrono::milliseconds freeze_time;
    std::chrono::milliseconds reconnect_age;
    context_t& context;
    std::unique_ptr<logging::logger_t> logger;
    asio::io_service& io_loop;
    asio::deadline_timer rebalance_timer;
    synchronized<peers_t> peers;
    api::vicodyn::balancer_ptr balancer;
};

} // namespace vicodyn
} // namespace cocaine
