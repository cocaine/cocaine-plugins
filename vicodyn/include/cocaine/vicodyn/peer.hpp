#pragma once

#include "cocaine/vicodyn/queue/invocation.hpp"
#include "cocaine/vicodyn/stream.hpp"

#include <asio/ip/tcp.hpp>

#include <future>

namespace cocaine {
namespace vicodyn {

class peer_t : public std::enable_shared_from_this<peer_t> {
public:
    enum class state_t {
        disconnected,
        connecting,
        connected,
        freezed
    };

    struct state_result_t {
        std::error_code ec;
        std::shared_ptr<peer_t> peer;
        state_t from;
        state_t to;
    };

    using state_callback_t = std::function<void(state_result_t)>;
    using endpoints_t = std::vector<asio::ip::tcp::endpoint>;
    using clock_t = std::chrono::system_clock;
    using message_t = io::aux::decoded_message_t;

    ~peer_t();

    peer_t(context_t& context, std::string service_name, asio::io_service& loop, endpoints_t endpoints, std::string uuid, bool local);

    auto invoke(const message_t& message, const io::graph_node_t& protocol, stream_ptr_t backward_stream) -> stream_ptr_t;

    auto connect() -> void;

    auto disconnect() -> void;

    auto freeze(clock_t::duration freeze_time) -> void;

    auto unfreeze() -> void;

    auto absorb(peer_t& failed) -> void;

    auto on_state_change(state_callback_t) -> void;

    auto state() const -> state_t;

    auto uuid() const -> const std::string&;

    auto local() const -> bool;

    auto freezed_till() const -> clock_t::time_point;

    auto last_used() const -> clock_t::time_point;

    auto endpoints() const -> const std::vector<asio::ip::tcp::endpoint>&;

private:
    auto switch_state(state_t expected_current_state, state_t desired_state) -> void;

    context_t& context;
    std::string service_name;
    asio::io_service& loop;
    std::unique_ptr<queue::invocation_t> queue;
    state_callback_t state_cb;
    std::unique_ptr<logging::logger_t> logger;

    struct {
        state_t state;
        std::string uuid;
        bool local;
        std::chrono::system_clock::time_point freezed_till;
        std::chrono::system_clock::time_point last_used;
        std::vector<asio::ip::tcp::endpoint> endpoints;
    } d;

};

class peers_t {
public:
    using peer_storage_t = std::map<std::string, std::shared_ptr<peer_t>>;

    peers_t();

    auto get(peer_t::state_t state) -> peer_storage_t&;

    auto get(peer_t::state_t state) const -> const peer_storage_t&;

    auto clear() -> void;

    auto add(std::string uuid, std::shared_ptr<peer_t> peer) -> bool;

    /// Returns nullptr if specified uuid was not found
    auto remove(const std::string& uuid) -> std::shared_ptr<peer_t>;

    auto migrate(peer_t::state_t from, peer_t::state_t to, const std::string uuid) -> void;

    auto size() const -> size_t;
private:
    std::map<peer_t::state_t, peer_storage_t> peer_groups;
};

} // namespace vicodyn
} // namespace cocaine
