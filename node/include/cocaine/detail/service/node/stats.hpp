#pragma once

#include <atomic>
#include <cstdint>

#include <metrics/accumulator/sliding/window.hpp>
#include <metrics/meter.hpp>
#include <metrics/timer.hpp>

#include <cocaine/locked_ptr.hpp>

namespace cocaine {

struct stats_t {
    struct {
        /// The number of requests, that are pushed into the queue.
        std::atomic<std::uint64_t> accepted;

        /// The number of requests, that were rejected due to queue overflow or other circumstances.
        std::atomic<std::uint64_t> rejected;
    } requests;

    struct {
        /// The number of successfully spawned slaves.
        std::atomic<std::uint64_t> spawned;

        /// The number of crashed slaves.
        std::atomic<std::uint64_t> crashed;
    } slaves;

    /// EWMA rates.
    std::unique_ptr<metrics::meter_t> meter;

    /// Channel processing time quantiles (summary).
    std::unique_ptr<metrics::timer<metrics::accumulator::sliding::window_t>> timer;

    stats_t();
};

}  // namespace cocaine
