#include "collector.hpp"

#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/numeric.hpp>

#include "cocaine/service/node/profile.hpp"

#include "cocaine/detail/service/node/slave/stats.hpp"

namespace cocaine {
namespace service {
namespace node {
namespace info {

namespace {

inline double
trunc(double v, uint n) noexcept {
    BOOST_ASSERT(n < 10);

    static const long long table[10] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};

    return std::ceil(v * table[n]) / table[n];
}

struct collector_t {
    std::size_t active;
    std::size_t cumload;

    explicit collector_t(const pool_type& pool) : active{}, cumload{} {
        for (const auto& it : pool) {
            const auto load = it.second.load();
            if (it.second.active() && load) {
                active++;
                cumload += load;
            }
        }
    }
};

} // namespace

info_collector_t::info_collector_t(cocaine::io::node::info::flags_t flags, dynamic_t::object_t* result)
    : flags(flags), result(*result) {}

void
info_collector_t::visit(std::int64_t accepted, std::int64_t rejected) const {
    dynamic_t::object_t info;

    info["accepted"] = accepted;
    info["rejected"] = rejected;

    result["requests"] = info;
}

void
info_collector_t::visit(const queue_t& value) {
    dynamic_t::object_t info;

    info["capacity"] = value.capacity;

    const auto now = std::chrono::high_resolution_clock::now();

    value.queue->apply([&](const queue_type& queue) {
        info["depth"] = queue.size();

        // Wake up aggregator.
        value.queue_depth.add(queue.size());
        info["depth_average"] = trunc(value.queue_depth.get(), 3);

        using value_type = queue_type::value_type;

        if (queue.empty()) {
            info["oldest_event_age"] = 0;
        } else {
            const auto min = *boost::min_element(
                queue | boost::adaptors::transformed(+[](const value_type& cur) { return cur.event.birthstamp; }));

            const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - min).count();

            info["oldest_event_age"] = duration;
        }
    });

    result["queue"] = info;
}

void
info_collector_t::visit(metrics::meter_t& meter) {
    dynamic_t::array_t info{{trunc(meter.m01rate(), 3), trunc(meter.m05rate(), 3), trunc(meter.m15rate(), 3)}};

    result["rate"] = info;
    result["rate_mean"] = trunc(meter.mean_rate(), 3);
}

template<typename Accumulate>
void
info_collector_t::visit(metrics::timer<Accumulate>& timer) {
    const auto snapshot = timer.snapshot();

    dynamic_t::object_t info;

    info["50.00%"] = trunc(snapshot.median() / 1e6, 3);
    info["75.00%"] = trunc(snapshot.p75() / 1e6, 3);
    info["90.00%"] = trunc(snapshot.p90() / 1e6, 3);
    info["95.00%"] = trunc(snapshot.p95() / 1e6, 3);
    info["98.00%"] = trunc(snapshot.p98() / 1e6, 3);
    info["99.00%"] = trunc(snapshot.p99() / 1e6, 3);
    info["99.95%"] = trunc(snapshot.value(0.9995) / 1e6, 3);

    result["timings"] = info;

    dynamic_t::object_t reversed;

    const auto find = [&](const double ms) -> double { return 100.0 * snapshot.phi(ms); };

    reversed["1ms"] = trunc(find(1e6), 2);
    reversed["2ms"] = trunc(find(2e6), 2);
    reversed["5ms"] = trunc(find(5e6), 2);
    reversed["10ms"] = trunc(find(10e6), 2);
    reversed["20ms"] = trunc(find(20e6), 2);
    reversed["50ms"] = trunc(find(50e6), 2);
    reversed["100ms"] = trunc(find(100e6), 2);
    reversed["500ms"] = trunc(find(500e6), 2);
    reversed["1000ms"] = trunc(find(1000e6), 2);

    result["timings_reversed"] = reversed;
}

void
info_collector_t::visit(const pool_t& value) {
    const auto now = std::chrono::high_resolution_clock::now();

    value.pool->apply([&](const pool_type& pool) {
        collector_t collector(pool);

        // Cumulative load on the app over all the slaves.
        result["load"] = collector.cumload;

        dynamic_t::object_t slaves;
        for (const auto& kv : pool) {
            const auto& name = kv.first;
            const auto& slave = kv.second;

            const auto stats = slave.stats();

            dynamic_t::object_t stat;
            stat["accepted"] = stats.total;
            stat["load:tx"] = stats.tx;
            stat["load:rx"] = stats.rx;
            stat["load:total"] = stats.load;
            stat["state"] = stats.state;
            stat["uptime"] = slave.uptime();

            // NOTE: Collects profile info.
            const auto profile = slave.profile();

            dynamic_t::object_t profile_info;
            profile_info["name"] = profile.name;
            if (flags & io::node::info::expand_profile) {
                profile_info["data"] = profile.object();
            }

            stat["profile"] = profile_info;

            if (stats.age) {
                const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - *stats.age).count();
                stat["oldest_channel_age"] = duration;
            } else {
                stat["oldest_channel_age"] = 0;
            }

            slaves[name] = stat;
        }

        dynamic_t::object_t pinfo;
        pinfo["active"] = collector.active;
        pinfo["idle"] = pool.size() - collector.active;
        pinfo["capacity"] = value.capacity;
        pinfo["slaves"] = slaves;
        pinfo["total:spawned"] = value.spawned;
        pinfo["total:crashed"] = value.crashed;

        result["pool"] = pinfo;
    });
}

template void info_collector_t::visit(metrics::timer<metrics::accumulator::decaying::exponentially_t>& timer);

} // namespace info
} // namespace node
} // namespace service
} // namespace cocaine
