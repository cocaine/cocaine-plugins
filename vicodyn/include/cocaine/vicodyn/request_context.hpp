#pragma once

#include "cocaine/vicodyn/peer.hpp"

#include <blackhole/logger.hpp>

namespace cocaine {
namespace vicodyn {

class request_context_t: public std::enable_shared_from_this<request_context_t> {
    using clock_t = std::chrono::system_clock;
    struct checkpoint_t {
        const char* message;
        size_t msg_len;
        clock_t::time_point when;
    };

    blackhole::logger_t& logger;
    clock_t::time_point start_time;
    std::atomic_flag closed;

    synchronized<std::vector<std::shared_ptr<peer_t>>> used_peers;
    synchronized<std::vector<checkpoint_t>> checkpoints;
    size_t retry_counter;

public:
    request_context_t(blackhole::logger_t& logger);

    ~request_context_t();

    auto mark_used_peer(std::shared_ptr<peer_t> peer) -> void;

    auto peer_use_count(const std::shared_ptr<peer_t>& peer) -> size_t;

    auto peer_use_count(const std::string& peer_uuid) -> size_t;

    auto register_retry() -> void;

    auto retry_count() -> size_t;

    template <size_t N>
    auto add_checkpoint(const char(&name)[N]) -> void {
        checkpoints->emplace_back(checkpoint_t{name, N - 1, std::chrono::system_clock::now()});
    }

    auto finish() -> void;

    auto fail(const std::error_code& ec, blackhole::string_view reason) -> void;

private:
    auto current_duration_ms() -> size_t;

    auto write(int level, const std::string& msg) -> void;
};

} // namespace vicodyn
} // namespace cocaine
