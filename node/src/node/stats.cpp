#include "cocaine/detail/service/node/stats.hpp"

#include <cmath>

#include <cocaine/context.hpp>
#include <cocaine/format.hpp>

#include <metrics/factory.hpp>
#include <metrics/registry.hpp>

namespace cocaine {

stats_t::stats_t(context_t& context, const std::string& name, std::chrono::high_resolution_clock::duration interval):
    meter(context.metrics_hub().meter(cocaine::format("{}.rate", name))),
    queue_depth(new metrics::usts::ewma_t(interval)),
    queue_depth_gauge(context.metrics_hub()
        .register_gauge<double>(cocaine::format("{}.queue.depth_average", name), {}, [&]() -> double {

            return queue_depth->get();
        }
    )),
    timer(context.metrics_hub().timer<metrics::accumulator::sliding::window_t>(cocaine::format("{}.timings", name)))
{
    queue_depth->add(0);

    requests.accepted = 0;
    requests.rejected = 0;

    slaves.spawned = 0;
    slaves.crashed = 0;
}

}  // namespace cocaine
