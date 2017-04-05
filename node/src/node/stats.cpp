#include "cocaine/detail/service/node/stats.hpp"

#include <cmath>

#include <cocaine/context.hpp>
#include <cocaine/format.hpp>

#include <metrics/factory.hpp>
#include <metrics/registry.hpp>

namespace cocaine {

stats_t::stats_t(context_t& context, const std::string& name, std::chrono::high_resolution_clock::duration interval):
    requests{
        context.metrics_hub().counter<std::int64_t>(cocaine::format("{}.requests.accepted", name)),
        context.metrics_hub().counter<std::int64_t>(cocaine::format("{}.requests.rejected", name))
    },
    slaves{
        context.metrics_hub().counter<std::int64_t>(cocaine::format("{}.slaves.spawned", name)),
        context.metrics_hub().counter<std::int64_t>(cocaine::format("{}.slaves.crashed", name))
    },
    meter(context.metrics_hub().meter(cocaine::format("{}.rate", name))),
    queue_depth(std::make_shared<metrics::usts::ewma_t>(interval)),
    queue_depth_gauge(context.metrics_hub()
        .register_gauge<double>(
            cocaine::format("{}.queue.depth_average", name),
            {},
            std::bind(&metrics::usts::ewma_t::get, queue_depth)
        )
    ),
    timer(context.metrics_hub().timer<metrics::accumulator::decaying::exponentially_t>(cocaine::format("{}.timings", name)))
{
    queue_depth->add(0);
}

}  // namespace cocaine
