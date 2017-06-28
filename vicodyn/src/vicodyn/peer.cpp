#include "cocaine/vicodyn/peer.hpp"

#include "cocaine/format/peer.hpp"
#include "cocaine/vicodyn/stream.hpp"

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

namespace cocaine {
namespace vicodyn {

peer_t::~peer_t(){
}

peer_t::peer_t(context_t& context, std::string service_name, asio::io_service& loop, endpoints_t endpoints, std::string uuid, bool local) :
    context(context),
    service_name(std::move(service_name)),
    loop(loop),
    queue(new queue::invocation_t),
    logger(context.log(format("vicodyn_peer/{}", uuid))),
    d({state_t::disconnected, std::move(uuid), local, clock_t::now(), clock_t::now(), std::move(endpoints)})
{}

auto peer_t::switch_state(state_t expected_current_state, state_t desired_state) -> void {
    if(d.state != expected_current_state) {
        throw error_t("invalid state - {}, should be {}", d.state, expected_current_state);
    }
    d.state = desired_state;
    COCAINE_LOG_DEBUG(logger, "switched peer {} state from {} to {}", uuid(), expected_current_state, desired_state);
    auto self = shared_from_this();
    loop.post([=](){
        self->state_cb(state_result_t{{}, shared_from_this(), expected_current_state, desired_state});
    });
}

auto peer_t::connect() -> void {
    COCAINE_LOG_DEBUG(logger, "connecting peer {} to {}", uuid(), endpoints());
    switch_state(state_t::disconnected, state_t::connecting);
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
                state_cb(state_result_t{ec, shared_from_this(), state_t::connecting, state_t::connecting});
            }
            return;
        }
        try {
            auto ptr = std::make_unique<asio::ip::tcp::socket>(std::move(*socket));
            //TODO: what can we do with shutdown and empty engines in context?

            auto session = context.engine().attach(std::move(ptr), nullptr);
            // queue will be in consistent state if exception is thrown
            // it is safe to reconnect peer to different endpoint
            auto counter = context.metrics_hub().counter<std::uint64_t>(format("vicodyn.{}.connections.counter", service_name));
            queue->attach(session_t::shared(std::move(session), std::move(counter)));
            switch_state(state_t::connecting, state_t::connected);
        } catch(const std::exception& e) {
            COCAINE_LOG_WARNING(logger, "failed to attach session to queue: {}", e.what());
            state_cb(state_result_t{ec, shared_from_this(), state_t::connecting, state_t::connecting});
        }
    });
}

auto peer_t::disconnect() -> void {
    queue->disconnect();
    switch_state(state_t::connected, state_t::disconnected);
    d.state = state_t::disconnected;
}

auto peer_t::freeze(clock_t::duration freeze_time) -> void {
    d.freezed_till = clock_t::now() + freeze_time;
    switch_state(state_t::disconnected, state_t::freezed);
}

auto peer_t::unfreeze() -> void {
    switch_state(state_t::freezed, state_t::disconnected);
}

auto peer_t::absorb(peer_t& failed) -> void {
    queue->absorb(*failed.queue);
}

auto peer_t::on_state_change(state_callback_t cb) -> void {
    state_cb = std::move(cb);
}

auto peer_t::state() const -> state_t {
    return d.state;
}

auto peer_t::uuid() const -> const std::string& {
    return d.uuid;
}

auto peer_t::local() const -> bool {
    return d.local;
}

auto peer_t::freezed_till() const -> clock_t::time_point {
    return d.freezed_till;
}

auto peer_t::last_used() const -> clock_t::time_point {
    return d.last_used;
}

auto peer_t::endpoints() const -> const std::vector<asio::ip::tcp::endpoint>& {
    return d.endpoints;
}

auto peer_t::invoke(const io::decoder_t::message_type& incoming_message,
                    const io::graph_node_t& protocol,
                    stream_ptr_t backward_stream) -> std::shared_ptr<stream_t>
{
    return queue->append(incoming_message.args(), incoming_message.type(), incoming_message.headers(), protocol,
                         std::move(backward_stream));
}

peers_t::peers_t() :
        peer_groups({{peer_t::state_t::disconnected, {}},
                     {peer_t::state_t::connecting, {}},
                     {peer_t::state_t::connected, {}},
                     {peer_t::state_t::freezed, {}}})
{}

auto peers_t::get(peer_t::state_t state) -> peer_storage_t& {
    return peer_groups.at(state);
}

auto peers_t::get(peer_t::state_t state) const -> const peer_storage_t& {
    return  peer_groups.at(state);
}

auto peers_t::clear() -> void {
    for(auto& group: peer_groups) {
        group.second.clear();
    }
}

auto peers_t::add(std::string uuid, std::shared_ptr<peer_t> peer) -> bool {
    return peer_groups.at(peer->state()).emplace(uuid, std::move(peer)).second;
}

/// Returns nullptr if specified uuid was not found
auto peers_t::remove(const std::string& uuid) -> std::shared_ptr<peer_t> {
    for(auto& group: peer_groups) {
        auto it = group.second.find(uuid);
        if(it != group.second.end()) {
            auto peer = it->second;
            group.second.erase(it);
            return peer;
        }
    }
    return nullptr;

}

auto peers_t::migrate(peer_t::state_t from, peer_t::state_t to, const std::string uuid) -> void {
    auto& group = peer_groups.at(from);
    auto it = group.find(uuid);
    if(it == group.end()) {
        throw error_t("could not find peer {} in state {}", uuid, from);
    }
    peer_groups.at(to)[uuid] = it->second;
    group.erase(it);
}

auto peers_t::size() const -> size_t {
    size_t total = 0;
    for(auto& group: peer_groups) {
        total += group.second.size();
    }
    return total;
}

} // namespace vicodyn
} // namespace cocaine
