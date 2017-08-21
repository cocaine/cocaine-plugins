#include "cocaine/vicodyn/peer.hpp"

#include "cocaine/format/peer.hpp"

#include <cocaine/context.hpp>
#include <cocaine/engine.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/rpc/asio/decoder.hpp>
#include <cocaine/rpc/graph.hpp>
#include <cocaine/rpc/session.hpp>
#include <cocaine/utility/future.hpp>
#include <cocaine/memory.hpp>

#include <asio/ip/tcp.hpp>
#include <asio/connect.hpp>

#include <blackhole/logger.hpp>

#include <metrics/registry.hpp>
#include <cocaine/vicodyn/peer.hpp>
#include <cocaine/rpc/upstream.hpp>

namespace cocaine {
namespace vicodyn {

peer_t::~peer_t(){
}

peer_t::peer_t(context_t& context, std::string service_name, asio::io_service& loop, endpoints_t endpoints, std::string uuid) :
    context(context),
    service_name(std::move(service_name)),
    loop(loop),
    timer(loop),
    logger(context.log(format("vicodyn_peer/{}", uuid))),
    d({std::move(uuid), std::move(endpoints)})
{}

auto peer_t::open_stream(std::shared_ptr<io::basic_dispatch_t> dispatch) -> io::upstream_ptr_t {
    if(!session) {
        throw error_t(error::not_connected, "session is not connected");
    }
    return session->fork(std::move(dispatch));
}

auto peer_t::schedule_reconnect() -> void {
    timer.expires_from_now(boost::posix_time::seconds(1));
    timer.async_wait([&](std::error_code ec){
        if(!ec) {
            connect();
        }
    });
}

auto peer_t::connect() -> void {
    COCAINE_LOG_DEBUG(logger, "connecting peer {} to {}", uuid(), endpoints());
    if(session) {
        try {
            session->detach(std::error_code());
        } catch (...) {}
    }

    auto socket = std::make_shared<asio::ip::tcp::socket>(loop);

    std::weak_ptr<peer_t> weak_self(shared_from_this());

    auto begin = d.endpoints.begin();
    auto end = d.endpoints.end();
    //TODO: save socket in  peer
    asio::async_connect(*socket, begin, end, [=](const std::error_code& ec, endpoints_t::const_iterator endpoint_it) {
        auto self = weak_self.lock();
        if(!self){
            return;
        }
        if(ec) {
            COCAINE_LOG_ERROR(logger, "could not connect to {} - {}({})", *endpoint_it, ec.message(), ec.value());
            if(endpoint_it == end) {
                schedule_reconnect();
            }
            return;
        }
        try {
            auto ptr = std::make_unique<asio::ip::tcp::socket>(std::move(*socket));
            session = context.engine().attach(std::move(ptr), nullptr);
        } catch(const std::exception& e) {
            COCAINE_LOG_WARNING(logger, "failed to attach session to queue: {}", e.what());
            schedule_reconnect();
        }
    });
}

auto peer_t::uuid() const -> const std::string& {
    return d.uuid;
}

auto peer_t::endpoints() const -> const std::vector<asio::ip::tcp::endpoint>& {
    return d.endpoints;
}

} // namespace vicodyn
} // namespace cocaine
