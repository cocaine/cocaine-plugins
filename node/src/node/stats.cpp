#include "cocaine/detail/service/node/stats.hpp"

#include <cmath>

#include <metrics/factory.hpp>

namespace cocaine {

stats_t::stats_t(std::chrono::high_resolution_clock::duration interval):
    meter(metrics::factory_t().meter()),
    queue_depth(new metrics::usts::ewma_t(interval)),
    timer(metrics::factory_t().timer<metrics::accumulator::sliding::window_t>())
{
    queue_depth->add(0);

    requests.accepted = 0;
    requests.rejected = 0;

    slaves.spawned = 0;
    slaves.crashed = 0;
}

}  // namespace cocaine
