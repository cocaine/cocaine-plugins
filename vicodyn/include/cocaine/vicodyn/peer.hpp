#pragma once

#include "cocaine/vicodyn/queue/send.hpp"
#include "cocaine/vicodyn/queue/invocation.hpp"

#include <asio/ip/tcp.hpp>

#include <future>

namespace cocaine {
namespace vicodyn {

class peer_t : public std::enable_shared_from_this<peer_t> {
public:
    typedef std::function<void(std::future<void>)> error_callback_t;
    typedef std::function<void()> connect_callback_t;

    ~peer_t();

    peer_t(context_t& context, asio::io_service& loop);

    auto absorb(peer_t&& peer) -> void;

    auto invoke(const io::aux::decoded_message_t& incoming_message,
                const io::graph_node_t& protocol,
                io::upstream_ptr_t downstream) -> std::shared_ptr<queue::send_t>;

    auto connect(std::vector<asio::ip::tcp::endpoint>) -> void;

    auto connected() -> bool;

    auto on_error(error_callback_t) -> void;

    auto on_connect(connect_callback_t) -> void;

private:
    context_t& context;
    asio::io_service& loop;
    std::vector<asio::ip::tcp::endpoint> endpoints;
    std::unique_ptr<logging::logger_t> logger;
    std::unique_ptr<queue::invocation_t> queue;

    connect_callback_t connect_cb;
    error_callback_t error_cb;
};

} // namespace vicodyn
} // namespace cocaine
