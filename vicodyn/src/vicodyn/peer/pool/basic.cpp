#include "cocaine/vicodyn/peer/pool/basic.hpp"

#include <cocaine/context.hpp>
#include <cocaine/context/signal.hpp>
#include <cocaine/dynamic.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/idl/context.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/rpc/dispatch.hpp>
#include <cocaine/traits/endpoint.hpp>
#include <cocaine/traits/vector.hpp>

#include <blackhole/logger.hpp>

#include <functional>
#include <cocaine/utility/future.hpp>
#include <cocaine/idl/context.hpp>

namespace cocaine {
namespace vicodyn {
namespace peer {
namespace pool {

namespace ph = std::placeholders;

namespace {
/// Chooses random element in [begin, end), satisfying Predicate p
/// Returns end in case end == begin or all elements are not satisfying predicate
template<class Iterator, class Predicate>
auto choose_random_if(Iterator begin, Iterator end, Predicate p) -> Iterator {
    size_t count = std::count_if(begin, end, p);
    if(count == 0) {
        return end;
    }
    size_t idx = rand() % count + 1;
    for(;begin != end; begin++){
        if(p(*begin)) {
            idx--;
        }
        if(idx == 0) {
            return begin;
        }
    }
    return begin;
}


/// Debug purpose function for printing current remotes in pool
auto print_remotes(const basic_t::remote_map_t& peers) -> std::string {
    std::ostringstream ss;
    ss << std::boolalpha << "[";
    for(auto peer_pair: peers) {
        ss << peer_pair.first << "\n connected: " << peer_pair.second.connected()
                              << ", has peer: " << static_cast<bool>(peer_pair.second.peer)
                              << ", active:" << peer_pair.second.active()
                              << "; \n";
    }
    ss << "]";
    return ss.str();
}

}

basic_t::basic_t(context_t& _context, asio::io_service& _io_loop, const std::string& name, const dynamic_t& args) :
        signal_dispatcher(std::make_shared<dispatch<io::context_tag>>("basic_pool_signal_dispatcher")),
        pool_size(args.as_object().at("pool_size", 3ul).as_uint()),
        retry_count(args.as_object().at("retry_count", 3ul).as_uint()),
        freeze_time(std::chrono::milliseconds(args.as_object().at("freeze_time_ms", 1000ul).as_uint())),
        reconnect_age(std::chrono::milliseconds(args.as_object().at("reconnect_age_ms", 15000ul).as_uint())),
        context(_context),
        logger(context.log(format("vicodyn_basic_pool/{}", name))),
        io_loop(_io_loop),
        rebalance_timer(io_loop)
{
    signal_dispatcher->on<io::context::shutdown>([=](){
        rebalance_timer.cancel();
        remotes->clear();
    });
    context.signal_hub().listen(signal_dispatcher, io_loop);
    rebalance_peers();
}

basic_t::~basic_t() = default;

auto basic_t::invoke(const io::aux::decoded_message_t& incoming_message,
                     const io::graph_node_t& protocol,
                     io::upstream_ptr_t downstream) -> std::shared_ptr<vicodyn::queue::send_t>
{
    std::shared_ptr<peer_t> peer;
    std::string uuid;
    for(size_t i = 0; i < retry_count; i++) {
        std::tie(uuid, peer) = choose_peer();
        try {
            COCAINE_LOG_DEBUG(logger, "processing invocation via {}", uuid);
            return peer->invoke(incoming_message, protocol, downstream);
        } catch(std::system_error& e) {
            on_peer_error(uuid, make_exceptional_future<void>());
        }
    }
    throw error_t(error::service_not_available, "failed to choose slot for invocation: all attemptes failed");
}

auto basic_t::choose_peer() -> std::pair<std::string, std::shared_ptr<peer_t>> {
    return remotes.apply([&](remote_map_t& remote_map) {
        COCAINE_LOG_DEBUG(logger, "choosing peer, remotes size: {}", remote_map.size());
        if(remote_map.empty()) {
            // this can happen when the proxy is shutting down
            throw error_t(error::service_not_available, "no active backend found");
        }
        // First we try to choose connected peer
        auto it = choose_random_if(remote_map.begin(), remote_map.end(), [](const remote_map_t::value_type& p){
            return p.second.connected();
        });
        if(it == remote_map.end()) {
            // No connected peer found - grab any and wait for connection to succeed
            it = choose_random_if(remote_map.begin(), remote_map.end(), [](const remote_map_t::value_type& p) {
                return static_cast<bool>(p.second.peer);
            });
            if(it == remote_map.end()) {
                throw error_t(error::service_not_available, "no peer found assigned to remote");
            }
        }
        return std::make_pair(it->first, it->second.peer);
    });
}

auto basic_t::on_peer_error(const std::string& uuid, std::future<void> future) -> void {
    try {
        future.get();
    } catch (const std::system_error& e) {
        COCAINE_LOG_WARNING(logger, "peer {} errored: {}", uuid, error::to_string(e));
    } catch (const std::exception& e) {
        COCAINE_LOG_WARNING(logger, "peer {} errored: {}", uuid, e.what());
    }

    remotes.apply([&](remote_map_t& remote_map){
        auto& remote = remote_map[uuid];
        remote.freezed_till = std::chrono::system_clock::now() + freeze_time;
        auto peer = remote.peer;
        remote.peer = nullptr;
        if(!peer) {
            COCAINE_LOG_ERROR(logger, "errored peer {} not found in map", uuid);
            return;
        }
        connect_peer(peer, remote_map);
    });
}

auto basic_t::connect_peer(std::shared_ptr<peer_t> peer, remote_map_t& remote_map) -> void {
    COCAINE_LOG_DEBUG(logger,"connecting peer");
    auto comp = [&](const remote_map_t::value_type& lhs, const remote_map_t::value_type& rhs){
        if(lhs.second.active() != rhs.second.active()) {
            return lhs.second.active();
        }
        if(!!lhs.second.peer != !!rhs.second.peer) {
            return static_cast<bool>(rhs.second.peer);
        }
        return lhs.second.last_used < rhs.second.last_used;
    };
    VICODYN_DEBUG(print_remotes(remote_map));
    auto it = std::min_element(remote_map.begin(), remote_map.end(), comp);
    if(it == remote_map.end() || !it->second.active() || it->second.peer) {
        COCAINE_LOG_WARNING(logger,"could not connect peer - no remotes");
        return;
    }
    it->second.last_used = std::chrono::system_clock::now();
    peer->on_connect([&](){
        rebalance_peers();
    });
    peer->on_error(std::bind(&basic_t::on_peer_error, this, it->second.uuid, ph::_1));
    peer->connect(it->second.endpoints);
    it->second.peer = std::move(peer);
    COCAINE_LOG_DEBUG(logger,"started peer connection");
    COCAINE_LOG_DEBUG(logger,"remotes: {}", print_remotes(remote_map));
}

auto basic_t::rebalance_peers() -> void {
    COCAINE_LOG_DEBUG(logger,"rebalancing peers");
    rebalance_timer.cancel();
    rebalance_timer.expires_from_now(boost::posix_time::milliseconds(reconnect_age.count()));
    rebalance_timer.async_wait([&](const std::error_code& ec){
        COCAINE_LOG_DEBUG(logger, "rebalance timer called");
        if(!ec) {
            rebalance_peers();
        }
    });
    remotes.apply([&](remote_map_t& remote_map){
        auto peer_counter = [](const remote_map_t& r_map) {
            return static_cast<size_t>(std::count_if(r_map.begin(), r_map.end(), [](const remote_map_t::value_type& p) {
                return static_cast<bool>(p.second.peer);
            }));
        };

        auto available_counter = [](const remote_map_t& r_map) -> size_t {
            return std::count_if(r_map.begin(), r_map.end(), [](const remote_map_t::value_type& p) {
                return p.second.active();
            });
        };

        // first evict old peer from pool
        if(peer_counter(remote_map) >= pool_size) {
            auto comp = [&](const remote_map_t::value_type& lhs, const remote_map_t::value_type& rhs) {
                if(!!lhs.second.peer == !!rhs.second.peer) {
                    return lhs.second.last_used < rhs.second.last_used;
                }
                if(lhs.second.peer) {
                    return true;
                } else {
                    return false;
                }
            };
            auto it = std::min_element(remote_map.begin(), remote_map.end(), comp);
            auto age = std::chrono::system_clock::now() - it->second.last_used;
            if(age > reconnect_age) {
                COCAINE_LOG_INFO(logger, "dropping out old peer {}, was connected {} ms ago",
                                 it->second.uuid,
                                 std::chrono::duration_cast<std::chrono::milliseconds>(age).count());
                it->second.peer = nullptr;
            }
            COCAINE_LOG_INFO(logger, "evicted old peer");
        } else {
            COCAINE_LOG_INFO(logger, "pool is not full, skipping eviction");
        }

        auto peer_count = peer_counter(remote_map);
        auto available_count = available_counter(remote_map);
        size_t new_count = std::min(pool_size, available_count) - peer_count;
        COCAINE_LOG_INFO(logger, "current active peer count: {}, connecting {} peers", peer_count, new_count);
        for(size_t i = 0; i < new_count; i++) {
            COCAINE_LOG_DEBUG(logger, "connecting one more peer");
            auto peer = std::make_shared<peer_t>(context, io_loop);
            connect_peer(peer, remote_map);
        }
    });
}

auto basic_t::register_real(std::string uuid, std::vector<asio::ip::tcp::endpoint> ep, bool local) -> void {
    COCAINE_LOG_DEBUG(logger,"registering real {}", uuid);
    auto now = std::chrono::system_clock::now();
    remotes->emplace(uuid, remote_t{uuid, ep, local, now, now, nullptr});
    rebalance_peers();
}

auto basic_t::deregister_real(const std::string& uuid) -> void{
    COCAINE_LOG_DEBUG(logger,"unregistering real {}", uuid);
    remotes->erase(uuid);
    rebalance_peers();
    COCAINE_LOG_DEBUG(logger, "remotes: {}", print_remotes(*remotes.synchronize()));
}

auto basic_t::size() -> size_t {
    return remotes->size();
}

} // namespace pool
} // namespace peer
} // namespace vicodyn
} // namespace cocaine
