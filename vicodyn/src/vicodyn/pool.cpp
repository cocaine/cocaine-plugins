#include "cocaine/vicodyn/pool.hpp"
#include "cocaine/format/peer.hpp"

#include <cocaine/context.hpp>
#include <cocaine/context/signal.hpp>
#include <cocaine/dynamic.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/format/map.hpp>
#include <cocaine/format/ptr.hpp>
#include <cocaine/idl/context.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/rpc/dispatch.hpp>
#include <cocaine/traits/endpoint.hpp>
#include <cocaine/traits/vector.hpp>

#include <blackhole/logger.hpp>

#include <functional>
#include <cocaine/utility/future.hpp>
#include <cocaine/idl/context.hpp>
#include <cocaine/vicodyn/stream.hpp>

namespace cocaine {
namespace vicodyn {

namespace ph = std::placeholders;

pool_t::pool_t(context_t& _context, asio::io_service& _io_loop, const std::string& name, const dynamic_t& args) :
        service_name(std::move(name)),
        signal_dispatcher(std::make_shared<dispatch<io::context_tag>>("basic_pool_signal_dispatcher")),
        pool_size(args.as_object().at("pool_size", 3ul).as_uint()),
        retry_count(args.as_object().at("retry_count", 3ul).as_uint()),
        freeze_time(std::chrono::milliseconds(args.as_object().at("freeze_time_ms", 1000ul).as_uint())),
        reconnect_age(std::chrono::milliseconds(args.as_object().at("reconnect_age_ms", 15000ul).as_uint())),
        context(_context),
        logger(context.log(format("vicodyn/pool/{}", service_name))),
        io_loop(_io_loop),
        rebalance_timer(io_loop),
        balancer(api::vicodyn::balancer(context, io_loop, args.as_object().at("balancer", "core").as_string(), name))
{
    signal_dispatcher->on<io::context::shutdown>([=](){
        rebalance_timer.cancel();
        peers->clear();
    });
    context.signal_hub().listen(signal_dispatcher, io_loop);
    rebalance_peers();
}

pool_t::~pool_t() = default;

auto pool_t::invoke(const message_t& message, const io::graph_node_t& protocol, stream_ptr_t backward_stream)
    -> stream_ptr_t
{
    return peers.apply([&](peers_t& peers){
        std::shared_ptr<peer_t> peer = balancer->choose_peer(message, peers);
        try {
            COCAINE_LOG_DEBUG(logger, "processing invocation via {}", peer);
            return peer->invoke(message, protocol, backward_stream);
        } catch(std::system_error& e) {
            COCAINE_LOG_WARNING(logger, "peer errored: {}, peer: {}", error::to_string(e), peer);
            peer->freeze(freeze_time);
        }
        throw error_t(error::service_not_available, "peer failed to proceed invocation request");
    });
}

auto pool_t::rebalance_peers() -> void {
    peers.apply([&](peers_t& peers){
        COCAINE_LOG_DEBUG(logger,"rebalancing peers, peermap - {}", peers);
        auto now = peer_t::clock_t::now();
        auto freezed = peers.get(peer_t::state_t::freezed);
        for(auto& peer_pair: freezed) {
            if(now > peer_pair.second->freezed_till()) {
                peer_pair.second->unfreeze();
            }
        }
        balancer->rebalance_peers(peers);
    });
    rebalance_timer.cancel();
    rebalance_timer.expires_from_now(boost::posix_time::milliseconds(reconnect_age.count()));
    rebalance_timer.async_wait([&](const std::error_code& ec){
        if(!ec) {
            rebalance_peers();
        }
    });
}

auto pool_t::on_peer_state(peer_t::state_result_t result) -> void {
    if(result.ec) {
        COCAINE_LOG_DEBUG(logger, "peer errored with {}", result.ec);
        result.peer->disconnect();
        result.peer->freeze(freeze_time);
    } else {
        COCAINE_LOG_DEBUG(logger, "migrating peer {} from {} to {}", result.peer, result.from, result.to);
        peers.apply([&](peers_t& peers) {
            peers.migrate(result.from, result.to, result.peer->uuid());
        });
    }
}

auto pool_t::register_real(std::string uuid, std::vector<asio::ip::tcp::endpoint> ep, bool local) -> void {
    COCAINE_LOG_DEBUG(logger,"registering real {}", uuid);
    auto peer = std::make_shared<peer_t>(context, service_name, io_loop, std::move(ep), uuid, local);
    //TODO: Is it safe to pass 'this' to this callback?
    peer->on_state_change(std::bind(&pool_t::on_peer_state, this, std::placeholders::_1));
    peers->add(std::move(uuid), std::move(peer));
    rebalance_peers();
}

auto pool_t::deregister_real(const std::string& uuid) -> void {
    COCAINE_LOG_DEBUG(logger,"unregistering real {}", uuid);
    auto peer = peers.apply([&](peers_t& peers) -> std::shared_ptr<peer_t> {
        return peers.remove(uuid);
    });
    if(!peer) {
        COCAINE_LOG_ERROR(logger, "peer with uuid {} not found in pool", uuid);
        return;
    }
    auto intercept_peer = balancer->choose_intercept_peer(*peers.synchronize());
    intercept_peer->absorb(*peer);
    COCAINE_LOG_DEBUG(logger, "peer {} absorbed {} load", intercept_peer, peer->uuid());
}

auto pool_t::size() -> size_t {
    return peers->size();
}

} // namespace vicodyn
} // namespace cocaine
