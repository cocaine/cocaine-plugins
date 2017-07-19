#include "cocaine/detail/service/node/stats.hpp"

#include <cmath>

#include <cocaine/context.hpp>
#include <cocaine/format.hpp>

#include <metrics/factory.hpp>
#include <metrics/registry.hpp>

namespace cocaine {

namespace {

const char name_requests_accepted[] = "{}.requests.accepted";
const char name_requests_rejected[] = "{}.requests.rejected";
const char name_slaves_spawned[] = "{}.slaves.spawned";
const char name_slaves_crashed[] = "{}.slaves.crashed";
const char name_rate[] = "{}.rate";
const char name_queue_depth_average[] = "{}.queue.depth_average";
const char name_timings[] = "{}.timings";

} // namespace

stats_t::stats_t(context_t& context, const std::string& name, std::chrono::high_resolution_clock::duration interval):
    name{name},
    metrics_hub(context.metrics_hub()),
    requests{
        metrics_hub.counter<std::int64_t>(cocaine::format(name_requests_accepted, name)),
        metrics_hub.counter<std::int64_t>(cocaine::format(name_requests_rejected, name))
    },
    slaves{
        metrics_hub.counter<std::int64_t>(cocaine::format(name_slaves_spawned, name)),
        metrics_hub.counter<std::int64_t>(cocaine::format(name_slaves_crashed, name))
    },
    meter(metrics_hub.meter(cocaine::format(name_rate, name))),
    queue_depth(std::make_shared<metrics::usts::ewma_t>(interval)),
    queue_depth_gauge(metrics_hub
        .register_gauge<double>(
            cocaine::format(name_queue_depth_average, name),
            {},
            std::bind(&metrics::usts::ewma_t::get, queue_depth)
        )
    ),
    timer(metrics_hub.timer<metrics::accumulator::decaying::exponentially_t>(cocaine::format(name_timings, name)))
{
    queue_depth->add(0);
}

auto stats_t::deregister() -> void {
    metrics_hub.remove<std::atomic<std::int64_t>>(cocaine::format(name_requests_accepted, name), {});
    metrics_hub.remove<std::atomic<std::int64_t>>(cocaine::format(name_requests_rejected, name), {});
    metrics_hub.remove<std::atomic<std::int64_t>>(cocaine::format(name_slaves_spawned, name), {});
    metrics_hub.remove<std::atomic<std::int64_t>>(cocaine::format(name_slaves_crashed, name), {});
    metrics_hub.remove<metrics::meter_t>(cocaine::format(name_rate, name), {});
    metrics_hub.remove<metrics::gauge<double>>(cocaine::format(name_queue_depth_average, name), {});
    metrics_hub.remove<metrics::timer<metrics::accumulator::decaying::exponentially_t>>(cocaine::format(name_timings, name), {});
}

} // namespace cocaine
