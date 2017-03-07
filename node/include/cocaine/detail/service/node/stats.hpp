#pragma once

#include <atomic>
#include <cstdint>

#include <metrics/accumulator/decaying/exponentially.hpp>
#include <metrics/meter.hpp>
#include <metrics/timer.hpp>
#include <metrics/usts/ewma.hpp>

#include <cocaine/forwards.hpp>
#include <cocaine/locked_ptr.hpp>

namespace cocaine {

struct stats_t {
    struct {
        /// Number of requests, that are pushed into the queue.
        metrics::shared_metric<std::atomic<std::int64_t>> accepted;

        /// Number of requests, that were rejected due to queue overflow or other circumstances.
        metrics::shared_metric<std::atomic<std::int64_t>> rejected;
    } requests;

    struct {
        /// Number of successfully spawned slaves.
        metrics::shared_metric<std::atomic<std::int64_t>> spawned;

        /// Number of crashed slaves.
        metrics::shared_metric<std::atomic<std::int64_t>> crashed;
    } slaves;

    /// EWMA rates.
    metrics::shared_metric<metrics::meter_t> meter;
    std::shared_ptr<metrics::usts::ewma_t> queue_depth;
    metrics::shared_metric<metrics::gauge<double>> queue_depth_gauge;

    /// Channel processing time quantiles (summary).
    metrics::shared_metric<metrics::timer<metrics::accumulator::decaying::exponentially_t>> timer;

    stats_t(context_t& context, const std::string& name, std::chrono::high_resolution_clock::duration interval);
};

}  // namespace cocaine
