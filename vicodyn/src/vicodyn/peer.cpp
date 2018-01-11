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
    session.apply([&](std::shared_ptr<session_t>& session) {
        if(session) {
            session->detach(std::error_code());
        }
    });
}

peer_t::peer_t(context_t& context, asio::io_service& loop, endpoints_t endpoints, std::string uuid, dynamic_t::object_t extra) :
    context(context),
    loop(loop),
    timer(loop),
    logger(context.log(format("vicodyn_peer/{}", uuid))),
    connecting(),
    d({std::move(uuid), std::move(endpoints), std::chrono::system_clock::now(), std::move(extra), {}})
{
    d.x_cocaine_cluster = d.extra.at("x-cocaine-cluster", "").as_string();
}

auto peer_t::schedule_reconnect() -> void {
    COCAINE_LOG_INFO(logger, "scheduling reconnection of peer {} to {}", uuid(), endpoints());
    session.apply([&](std::shared_ptr<session_t>& session) {
        schedule_reconnect(session);
    });
}
auto peer_t::schedule_reconnect(std::shared_ptr<cocaine::session_t>& session) -> void {
    if(connecting) {
        COCAINE_LOG_INFO(logger, "reconnection is alredy in progress for {}", uuid());
        return;
    }
    if(session) {
        // In fact it should be detached already
        session->detach(std::error_code());
        session = nullptr;
    }
    timer.expires_from_now(boost::posix_time::seconds(1));
    timer.async_wait([&](std::error_code ec) {
        if(!ec) {
            connect();
        }
    });
    connecting = true;
    COCAINE_LOG_INFO(logger, "scheduled reconnection of peer {} to {}", uuid(), endpoints());
}

auto peer_t::connect() -> void {
    connecting = true;
    COCAINE_LOG_INFO(logger, "connecting peer {} to {}", uuid(), endpoints());

    auto socket = std::make_shared<asio::ip::tcp::socket>(loop);
    auto connect_timer = std::make_shared<asio::deadline_timer>(loop);

    std::weak_ptr<peer_t> weak_self(shared_from_this());

    auto begin = d.endpoints.begin();
    auto end = d.endpoints.end();

    connect_timer->expires_from_now(boost::posix_time::seconds(60));
    connect_timer->async_wait([=](std::error_code ec) {
        auto self = weak_self.lock();
        if(!self){
            return;
        }
        if(!ec) {
            COCAINE_LOG_INFO(logger, "connection timer expired, canceling socket, going to schedule reconnect");
            socket->cancel();
            self->connecting = false;
            self->schedule_reconnect();
        } else {
            COCAINE_LOG_DEBUG(logger, "connection timer was cancelled");
        }
    });

    asio::async_connect(*socket, begin, end, [=](const std::error_code& ec, endpoints_t::const_iterator endpoint_it) {
        auto self = weak_self.lock();
        if(!self){
            return;
        }
        COCAINE_LOG_DEBUG(logger, "cancelling timer");
        if(!connect_timer->cancel()) {
            COCAINE_LOG_ERROR(logger, "could not connect to {} - timed out (timer could not be cancelled)", *endpoint_it);
            return;
        }
        if(ec) {
            COCAINE_LOG_ERROR(logger, "could not connect to {} - {}({})", *endpoint_it, ec.message(), ec.value());
            if(endpoint_it == end) {
                connecting = false;
                schedule_reconnect();
            }
            return;
        }
        try {
            COCAINE_LOG_INFO(logger, "suceesfully connected peer {} to {}", uuid(), endpoints());
            auto ptr = std::make_unique<asio::ip::tcp::socket>(std::move(*socket));
            auto new_session = context.engine().attach(std::move(ptr), nullptr);
            session.apply([&](std::shared_ptr<session_t>& session) {
                connecting = false;
                session = std::move(new_session);
                d.last_active = std::chrono::system_clock::now();
            });
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

auto peer_t::connected() const -> bool {
    return session.apply([&](const std::shared_ptr<session_t>& session){
        return static_cast<bool>(session);
    });
}

auto peer_t::last_active() const -> std::chrono::system_clock::time_point {
    return d.last_active;
}

auto peer_t::extra() const -> const dynamic_t::object_t& {
    return d.extra;
}

auto peer_t::x_cocaine_cluster() const -> const std::string& {
    return d.x_cocaine_cluster;
}

peers_t::peers_t(context_t& context):
    context(context),
    logger(context.log("vicodyn/peers_t"))
{}

auto peers_t::register_peer(const std::string& uuid, const endpoints_t& endpoints, dynamic_t::object_t extra)
    -> std::shared_ptr<peer_t>
{
    return apply([&](data_t& data){
        auto& peer = data.peers[uuid];
        if(!peer) {
            peer = std::make_shared<peer_t>(context, executor.asio(), endpoints, uuid, std::move(extra));
            peer->connect();
        } else if (endpoints != peer->endpoints()) {
            COCAINE_LOG_ERROR(logger, "changed endpoints detected for uuid {}, previous {}, new {}", uuid,
                              peer->endpoints(), endpoints);
            peer = std::make_shared<peer_t>(context, executor.asio(), endpoints, uuid, extra);
            peer->connect();
        }
        return peer;
    });
}

auto peers_t::register_peer(const std::string& uuid, std::shared_ptr<peer_t> peer) -> void {
    apply([&](data_t& data) {
        data.peers[uuid] = std::move(peer);
    });
}

auto peers_t::erase_peer(const std::string& uuid) -> void {
    apply([&](data_t& data){
        data.peers.erase(uuid);
    });
}

auto peers_t::register_app(const std::string& uuid, const std::string& name) -> void {
    apply([&](data_t& data) {
        data.apps[name].insert(uuid);
    });
}

auto peers_t::erase_app(const std::string& uuid, const std::string& name) -> void {
    apply([&](data_t& data) {
        data.apps[name].erase(uuid);
    });
}

auto peers_t::erase(const std::string& uuid) -> void {
    erase_peer(uuid);
    apply([&](data_t& data) {
        data.peers.erase(uuid);
        for(auto pair : data.apps) {
            pair.second.erase(uuid);
        }
    });
}

auto peers_t::peer(const std::string& uuid) -> std::shared_ptr<peer_t> {
    return apply_shared([&](const data_t& data) -> std::shared_ptr<peer_t>{
        auto it = data.peers.find(uuid);
        if (it != data.peers.end()) {
            return it->second;
        }
        return nullptr;
    });
}

} // namespace vicodyn
} // namespace cocaine
