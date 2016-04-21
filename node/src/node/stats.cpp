#include "cocaine/detail/service/node/stats.hpp"

#include <cmath>

#include <metrics/factory.hpp>

namespace cocaine {

stats_t::stats_t():
    meter(metrics::factory_t().meter()),
    timer(metrics::factory_t().timer<metrics::accumulator::sliding::window_t>())
{
    requests.accepted = 0;
    requests.rejected = 0;

    slaves.spawned = 0;
    slaves.crashed = 0;
}

}  // namespace cocaine
