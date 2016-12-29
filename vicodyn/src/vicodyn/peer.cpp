#include "cocaine/vicodyn/peer.hpp"

#include <cocaine/context.hpp>
#include <cocaine/engine.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/rpc/asio/decoder.hpp>
#include <cocaine/rpc/graph.hpp>
#include <cocaine/rpc/session.hpp>
#include <cocaine/utility/future.hpp>
#include <cocaine/memory.hpp>

#include <asio/ip/tcp.hpp>
#include <asio/connect.hpp>

#include <blackhole/logger.hpp>

namespace cocaine {
namespace vicodyn {

peer_t::~peer_t() = default;

peer_t::peer_t(context_t& _context, asio::io_service& _loop) :
    context(_context),
    loop(_loop),
    logger(context.log("vicodyn_peer")),
    queue(new queue::invocation_t)
{}

auto peer_t::absorb(peer_t&& peer) -> void {
    queue->absorb(std::move(*peer.queue));
}

auto peer_t::invoke(const io::decoder_t::message_type& incoming_message,
                    const io::graph_node_t& protocol,
                    io::upstream_ptr_t downstream) -> std::shared_ptr<queue::send_t>
{
    assert(error_cb);

    return queue->append(incoming_message.args(), incoming_message.type(), incoming_message.headers(), protocol, downstream);
}

auto peer_t::connected() -> bool {
    return queue->connected();
}

auto peer_t::connect(std::vector<asio::ip::tcp::endpoint> _endpoints) -> void {
    assert(error_cb);
    endpoints = std::move(_endpoints);
    auto socket = std::make_shared<asio::ip::tcp::socket>(loop);

    std::weak_ptr<peer_t> weak_self(shared_from_this());

    //TODO: save socket in  peer
    asio::async_connect(*socket, endpoints.begin(), endpoints.end(),
        [=](const std::error_code& ec, std::vector<asio::ip::tcp::endpoint>::const_iterator /*endpoint*/) {
            auto self = weak_self.lock();
            if(!self){
                VICODYN_DEBUG("peer disappeared during connection");
                return;
            }
            if(ec) {
                COCAINE_LOG_ERROR(logger, "could not connect - {}({})", ec.message(), ec.value());
                error_cb(make_exceptional_future<void>(ec));
                return;
            }
            try {
                if(connect_cb) {
                    connect_cb();
                }
                auto ptr = std::make_unique<asio::ip::tcp::socket>(std::move(*socket));
                //TODO: what can we do with shutdown and empty engines in context?
                auto session = context.engine().attach(std::move(ptr), nullptr);
                // queue will be in consistent state if exception is thrown
                // it is safe to reconnect peer to different endpoint
                queue->attach(std::move(session));
            } catch(const std::exception& e) {
                COCAINE_LOG_WARNING(logger, "failed to attach session to queue: {}", e.what());
                error_cb(make_exceptional_future<void>());
            }
        });
}

auto peer_t::on_connect(connect_callback_t cb) -> void {
    connect_cb = std::move(cb);
}

auto peer_t::on_error(error_callback_t cb) -> void {
    error_cb = std::move(cb);
}

} // namespace vicodyn
} // namespace cocaine
