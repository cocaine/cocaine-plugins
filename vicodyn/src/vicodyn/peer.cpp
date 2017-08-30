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

peer_t::peer_t(context_t& context, asio::io_service& loop, endpoints_t endpoints, std::string uuid) :
    context(context),
    loop(loop),
    timer(loop),
    logger(context.log(format("vicodyn_peer/{}", uuid))),
    connecting(),
    d({std::move(uuid), std::move(endpoints)})
{}

auto peer_t::open_stream(std::shared_ptr<io::basic_dispatch_t> dispatch) -> io::upstream_ptr_t {
    return session.apply([&](std::shared_ptr<session_t>& session) {
        if(!session) {
            throw error_t(error::not_connected, "session is not connected");
        }
        d.last_active = std::chrono::system_clock::now();
        return session->fork(std::move(dispatch));
    });
}

auto peer_t::schedule_reconnect() -> void {
    COCAINE_LOG_DEBUG(logger, "scheduling reconnection of peer {} to {}", uuid(), endpoints());
    session.apply([&](std::shared_ptr<session_t>& session) {
        if(connecting) {
            COCAINE_LOG_DEBUG(logger, "reconnection is alredy in progress for {}", uuid());
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
    });
}

auto peer_t::connect() -> void {
    connecting = true;
    COCAINE_LOG_INFO(logger, "connecting peer {} to {}", uuid(), endpoints());

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
                connecting = false;
                schedule_reconnect();
            }
            return;
        }
        try {
            COCAINE_LOG_INFO(logger, "connected peer {} to {}", uuid(), endpoints());
            auto ptr = std::make_unique<asio::ip::tcp::socket>(std::move(*socket));
            session.apply([&](std::shared_ptr<session_t>& session) {
                connecting = false;
                session = context.engine().attach(std::move(ptr), nullptr);
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

peers_t::peers_t(context_t& context):
    context(context),
    logger(context.log("vicodyn/peers_t"))
{}

auto peers_t::register_peer(const std::string& uuid, const endpoints_t& endpoints) -> std::shared_ptr<peer_t> {
    return data.apply([&](data_t& data){
        auto& peer = data.peers[uuid];
        if(!peer) {
            peer = std::make_shared<peer_t>(context, executor.asio(), endpoints, uuid);
            peer->connect();
        } else if (endpoints != peer->endpoints()) {
            COCAINE_LOG_ERROR(logger, "changed endpoints detected for uuid {}, previous {}, new {}", uuid,
                              peer->endpoints(), endpoints);
            peer = std::make_shared<peer_t>(context, executor.asio(), endpoints, uuid);
            peer->connect();
        }
        return peer;
    });
}

auto peers_t::register_peer(const std::string& uuid, std::shared_ptr<peer_t> peer) -> void {
    data.apply([&](data_t& data) {
        data.peers[uuid] = std::move(peer);
    });
}

auto peers_t::erase_peer(const std::string& uuid) -> void {
    data.apply([&](data_t& data){
        data.peers.erase(uuid);
    });
}

auto peers_t::register_app(const std::string& uuid, const std::string& name) -> void {
    data.apply([&](data_t& data) {
        auto& apps = data.apps[name];
        auto it = std::find(apps.begin(), apps.end(), uuid);
        if(it == apps.end()) {
            apps.push_back(uuid);
        }
    });
}

auto peers_t::erase_app(const std::string& uuid, const std::string& name) -> void {
    data.apply([&](data_t& data) {
        auto& apps = data.apps[name];
        auto it = std::remove(apps.begin(), apps.end(), uuid);
        apps.resize(it - apps.begin());
        if(apps.empty()) {
            data.apps.erase(name);
        }
    });
}

auto peers_t::erase(const std::string& uuid) -> void {
    erase_peer(uuid);
    data.apply([&](data_t& data) {
        for(auto it = data.apps.begin(); it != data.apps.end();) {
            auto inner_it = std::remove(it->second.begin(), it->second.end(), uuid);
            it->second.resize(inner_it - it->second.begin());
            if(it->second.empty()) {
                it = data.apps.erase(it);
            } else {
                it++;
            }
        }
    });
}

auto peers_t::inner() -> synchronized<data_t>& {
    return data;
}

} // namespace vicodyn
} // namespace cocaine
