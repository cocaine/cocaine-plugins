#include "cocaine/vicodyn/balancer/simple.hpp"

#include "cocaine/format/peer.hpp"
#include "cocaine/vicodyn/peer.hpp"

#include <cocaine/context.hpp>
#include <cocaine/dynamic.hpp>
#include <cocaine/errors.hpp>

namespace cocaine {
namespace vicodyn {
namespace balancer {

simple_t::simple_t(context_t& context, asio::io_service& io_service, const std::string& service_name, const dynamic_t& conf) :
    balancer_t(context, io_service, service_name, conf),
    conf(conf),
    logger(context.log(format("vicodyn/balancer/simple/{}", service_name)))
{}

/// Process invocation inside pool. Peer selecting logic is usually applied before invocation.
auto simple_t::choose_peer(const message_t&, const cocaine::vicodyn::peers_t& peers) -> std::shared_ptr<peer_t> {
    return choose_peer(peers);
}

auto simple_t::choose_peer(const cocaine::vicodyn::peers_t& peers) -> std::shared_ptr<peer_t> {
    using state_t = peer_t::state_t;
    const peers_t::peer_storage_t* peer_group;
    state_t chosen_state;
    for(auto state: {state_t::connected, state_t::connecting, state_t::disconnected}) {
        chosen_state = state;
        peer_group = &peers.get(state);
        if(!peer_group->empty()) {
            break;
        }
        COCAINE_LOG_WARNING(logger, "no {} peers found", state);
    }
    if(peer_group->empty()) {
        throw error_t("no suitable peers found - all peers are freezed");
    }

    COCAINE_LOG_DEBUG(logger, "choosing randomly from {} {} peers", peer_group->size(), chosen_state);
    auto it = std::begin(*peer_group);
    std::advance(it, rand() % peer_group->size());
    return it->second;
}

auto simple_t::choose_intercept_peer(const cocaine::vicodyn::peers_t& peers)
        -> std::shared_ptr<cocaine::vicodyn::peer_t>
{
    return choose_peer(peers);
}

auto simple_t::rebalance_peers(const cocaine::vicodyn::peers_t& peers) -> void {
    auto pool_size = conf.as_object().at("pool_size", 4u).as_uint();
    auto connected = peers.get(peer_t::state_t::connected);
    auto connecting = peers.get(peer_t::state_t::connecting);
    auto disconnected = peers.get(peer_t::state_t::disconnected);
    int64_t disconnected_cnt = static_cast<int64_t>(disconnected.size());
    int64_t to_connect = pool_size - connected.size() - connecting.size();
    if(to_connect <= 0 && !disconnected.empty()) {
        auto it = std::begin(connected);
        std::advance(it, rand() % connected.size());
        it->second->disconnect();
        to_connect++;
    }

    to_connect = std::min(disconnected_cnt, to_connect);
    for(int64_t i = 0; i < to_connect; i++) {
        auto it = std::begin(disconnected);
        std::advance(it, rand() % disconnected.size());
        if(it->second->state() == peer_t::state_t::disconnected) {
            it->second->connect();
        } else {
            COCAINE_LOG_DEBUG(logger, "missed, got {}, leaving this till next rebalance", it->second->state());
        }
    }

}

} // namespace balancer
} // namespace vicodyn
} // namespace cocaine
