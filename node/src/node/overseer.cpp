#include "cocaine/detail/service/node/overseer.hpp"

#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>

#include <blackhole/scoped_attributes.hpp>

#include "cocaine/context.hpp"

#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"

#include "cocaine/detail/service/node/dispatch/client.hpp"
#include "cocaine/detail/service/node/dispatch/handshake.hpp"
#include "cocaine/detail/service/node/dispatch/worker.hpp"
#include "cocaine/detail/service/node/slave/control.hpp"
#include "cocaine/detail/service/node/slot.hpp"
#include "cocaine/detail/service/node/util.hpp"

#include <boost/accumulators/statistics/extended_p_square.hpp>

#include <metrics/accumulator/sliding/window.hpp>
#include <metrics/counter.hpp>
#include <metrics/registry.hpp>
#include <metrics/timer.hpp>

namespace ph = std::placeholders;

using namespace cocaine;

namespace cocaine {
namespace {

template<typename T>
struct visitor {
    template<typename Visitable>
    void visit(Visitable& visitable) {
        visitable.visit(this);
    }
};

}  // namespace
}  // namespace cocaine

namespace cocaine {

struct metrics_t : public visitor<metrics_t>  {
    struct requests_t : public visitor<requests_t> {
        /// The number of requests, that are pushed into the queue.
        metrics::counter<std::uint64_t> accepted;
        /// The number of requests, that were rejected due to queue overflow or other circumstances.
        metrics::counter<std::uint64_t> rejected;

        requests_t(metrics::registry_t& re, const std::string& name):
            accepted(re.counter<std::uint64_t>(format("cocaine.%s.requests.accepted", name))),
            rejected(re.counter<std::uint64_t>(format("cocaine.%s.requests.rejected", name)))
        {}
    };

    struct slaves_t : public visitor<slaves_t> {
        /// The number of successfully spawned slaves.
        metrics::counter<std::uint64_t> spawned;
        /// The number of crashed slaves.
        metrics::counter<std::uint64_t> crashed;

        slaves_t(metrics::registry_t& re, const std::string& name):
            spawned(re.counter<std::uint64_t>(format("cocaine.%s.slaves.spawned", name))),
            crashed(re.counter<std::uint64_t>(format("cocaine.%s.slaves.crashed", name)))
        {}
    };

    struct timers_t : public visitor<timers_t> {
        typedef metrics::timer<metrics::accumulator::sliding::window_t> timer_type;

        /// Overall event processing time distribution measured from the event enqueueing until
        /// close event (queue time + slave time).
        timer_type overall;

        timers_t(metrics::registry_t& re, const std::string& name, const std::string& event):
            overall(re.timer(format("cocaine.%s.timers.%s.overall", name, event)))
        {}
    };

    requests_t requests;
    slaves_t slaves;
    synchronized<std::map<std::string, timers_t>> timers;

    metrics_t(metrics::registry_t& registry, const std::string& name):
        requests(registry, name),
        slaves(registry, name)
    {}
};

}  // namespace cocaine

struct collector_t {
    std::size_t active;
    std::size_t cumload;

    explicit
    collector_t(const overseer_t::pool_type& pool):
        active{},
        cumload{}
    {
        for (const auto& it : pool) {
            const auto load = it.second.load(); // TODO: Replace with range-based for + values.
            if (it.second.active() && load) {
                active++;
                cumload += load;
            }
        }
    }
};

overseer_t::overseer_t(context_t& context,
                       manifest_t manifest,
                       profile_t profile,
                       std::shared_ptr<asio::io_service> loop):
    log(context.log(format("%s/overseer", manifest.name))),
    context(context),
    birthstamp(std::chrono::system_clock::now()),
    manifest_(std::move(manifest)),
    profile_(profile),
    loop(loop),
    pool_target{},
    stats{},
    metrics(new metrics_t(context.metrics(), manifest_.name))
{
    COCAINE_LOG_DEBUG(log, "overseer has been initialized");
}

overseer_t::~overseer_t() {
    COCAINE_LOG_DEBUG(log, "overseer has been destroyed");
}

manifest_t
overseer_t::manifest() const {
    return manifest_;
}

profile_t
overseer_t::profile() const {
    return *profile_.synchronize();
}

namespace {

// Helper tagged struct.
struct queue_t {
    unsigned long capacity;

    const synchronized<overseer_t::queue_type>* queue;
};

// Helper tagged struct.
struct pool_t {
    unsigned long capacity;

    metrics_t::slaves_t& slaves;

    const synchronized<overseer_t::pool_type>* pool;
};

class info_visitor_t {
    const io::node::info::flags_t flags;

    dynamic_t::object_t& result;

public:
    info_visitor_t(io::node::info::flags_t flags, dynamic_t::object_t* result):
        flags(flags),
        result(*result)
    {}

    void
    visit(const manifest_t& value) {
        if (flags & io::node::info::expand_manifest) {
            result["manifest"] = value.object();
        }
    }

    void
    visit(const profile_t& value) {
        dynamic_t::object_t info;

        // Useful when you want to edit the profile.
        info["name"] = value.name;

        if (flags & io::node::info::expand_profile) {
            info["data"] = value.object();
        }

        result["profile"] = info;
    }

    // Incoming requests.
    void
    visit(metrics_t::requests_t& requests) {
        dynamic_t::object_t info;

        info["accepted"] = requests.accepted.get();
        info["rejected"] = requests.rejected.get();

        result["requests"] = info;
    }

    // Pending events queue.
    void
    visit(const queue_t& value) {
        dynamic_t::object_t info;

        info["capacity"] = value.capacity;

        const auto now = std::chrono::high_resolution_clock::now();

        value.queue->apply([&](const overseer_t::queue_type& queue) {
            info["depth"] = queue.size();

            typedef overseer_t::queue_type::value_type value_type;

            if (queue.empty()) {
                info["oldest_event_age"] = 0;
            } else {
                const auto min = *boost::min_element(queue |
                    boost::adaptors::transformed(+[](const value_type& cur) {
                        return cur->event.birthstamp;
                    })
                );

                const auto duration = std::chrono::duration_cast<
                    std::chrono::milliseconds
                >(now - min).count();

                info["oldest_event_age"] = duration;
            }
        });

        result["queue"] = info;
    }

    void
    visit(const std::map<std::string, metrics_t::timers_t>& distributions) {
        dynamic_t::object_t info;

        for (const auto& kv : distributions) {
            const auto& event = kv.first;
            const auto& timers = kv.second;

            const auto snapshot = timers.overall.snapshot();
            dynamic_t::object_t metrics;

            metrics["p50"] = std::lround(snapshot.median());
            metrics["p75"] = std::lround(snapshot.p75());
            metrics["p90"] = std::lround(snapshot.p90());
            metrics["p95"] = std::lround(snapshot.p95());
            metrics["p98"] = std::lround(snapshot.p98());
            metrics["p99"] = std::lround(snapshot.p99());

            info[event] = metrics;
        }

        result["timings"] = info;
    }

    void
    visit(const pool_t& value) {
        const auto now = std::chrono::high_resolution_clock::now();

        value.pool->apply([&](const overseer_t::pool_type& pool) {
            collector_t collector(pool);

            // Cumulative load on the app over all the slaves.
            result["load"] = collector.cumload;

            dynamic_t::object_t slaves;
            for (const auto& kv : pool) {
                const auto& name = kv.first;
                const auto& slave = kv.second;

                const auto stats = slave.stats();

                dynamic_t::object_t stat;
                stat["accepted"]   = stats.total;
                stat["load:tx"]    = stats.tx;
                stat["load:rx"]    = stats.rx;
                stat["load:total"] = stats.load;
                stat["state"]      = stats.state;
                stat["uptime"]     = slave.uptime();

                // NOTE: Collects profile info.
                const auto profile = slave.profile();

                dynamic_t::object_t profile_info;
                profile_info["name"] = profile.name;
                if (flags & io::node::info::expand_profile) {
                    profile_info["data"] = profile.object();
                }

                stat["profile"] = profile_info;

                if (stats.age) {
                    const auto duration = std::chrono::duration_cast<
                        std::chrono::milliseconds
                    >(now - *stats.age).count();
                    stat["oldest_channel_age"] = duration;
                } else {
                    stat["oldest_channel_age"] = 0;
                }

                slaves[name] = stat;
            }

            dynamic_t::object_t pinfo;
            pinfo["active"]   = collector.active;
            pinfo["idle"]     = pool.size() - collector.active;
            pinfo["capacity"] = value.capacity;
            pinfo["slaves"]   = slaves;
            pinfo["total:spawned"] = value.slaves.spawned.get();
            pinfo["total:crashed"] = value.slaves.crashed.get();

            result["pool"] = pinfo;
        });
    }
};

} // namespace

dynamic_t::object_t
overseer_t::info(io::node::info::flags_t flags) const {
    dynamic_t::object_t result;

    result["uptime"] = uptime().count();

    info_visitor_t visitor(flags, &result);
    visitor.visit(manifest());
    visitor.visit(profile());
    visitor.visit(metrics->requests);
    visitor.visit({ profile().queue_limit, &queue });
    const auto distributions = *metrics->timers.synchronize();
    visitor.visit(distributions);
    visitor.visit({ profile().pool_limit, metrics->slaves, &pool });

    return result;
}

std::chrono::seconds
overseer_t::uptime() const {
    const auto now = std::chrono::system_clock::now();

    return std::chrono::duration_cast<std::chrono::seconds>(now - birthstamp);
}

void
overseer_t::keep_alive(int count) {
    count = std::max(0, count);
    COCAINE_LOG_DEBUG(log, "changed keep-alive slave count to %d", count);

    pool_target = count;
    rebalance_slaves();
}

namespace {

template<typename SuccCounter, typename FailCounter, typename F>
void count_success_if(SuccCounter* succ, FailCounter* fail, F fn) {
    try {
        fn();
        succ->inc();
    } catch (...) {
        fail->inc();
        throw;
    }
}

} // namespace

std::shared_ptr<client_rpc_dispatch_t>
overseer_t::enqueue(io::streaming_slot<io::app::enqueue>::upstream_type downstream,
                    app::event_t event,
                    boost::optional<service::node::slave::id_t> /*id*/)
{
    // TODO: Handle id parameter somehow.

    std::shared_ptr<client_rpc_dispatch_t> dispatch;

    count_success_if(&metrics->requests.accepted, &metrics->requests.rejected, [&] {
        queue.apply([&](queue_type& queue) {
            const auto limit = profile().queue_limit;

            if (queue.size() >= limit && limit > 0) {
                throw std::system_error(error::queue_is_full);
            }

            dispatch = std::make_shared<client_rpc_dispatch_t>(manifest().name);

            queue.push_back({
                {std::move(event), dispatch, std::move(downstream)},
                trace_t::current()
            });
        });
    });

    rebalance_events();
    rebalance_slaves();

    return dispatch;
}

io::dispatch_ptr_t
overseer_t::prototype() {
    return std::make_shared<const handshake_t>(
        manifest().name,
        std::bind(&overseer_t::on_handshake, shared_from_this(), ph::_1, ph::_2, ph::_3)
    );
}

void
overseer_t::spawn(pool_type& pool) {
    COCAINE_LOG_INFO(log, "enlarging the slaves pool to %d", pool.size() + 1);

    slave_context ctx(context, manifest(), profile());

    // It is guaranteed that the cleanup handler will not be invoked from within the slave's
    // constructor.
    const auto uuid = ctx.id;
    pool.insert(std::make_pair(
        uuid,
        slave_t(std::move(ctx), *loop, std::bind(&overseer_t::on_slave_death, shared_from_this(), ph::_1, uuid))
    ));

    metrics->slaves.spawned.inc();
}

namespace cpp17 {
namespace map {
namespace {

template<typename M, typename... Args>
std::pair<typename M::iterator, bool>
try_emplace(M* map, const typename M::key_type& key, Args&&... args) {
    auto it = map->find(key);

    if (it != map->end()) {
        return std::make_pair(it, false);
    }

    return map->insert(std::make_pair(key, typename M::mapped_type(std::forward<Args>(args)...)));
}

}  // namespace
}  // namespace map
}  // namespace cpp17

void
overseer_t::assign(slave_t& slave, slave::channel_t& payload) {
    typedef metrics_t::timers_t::timer_type timer_type;

    auto observer = std::make_shared<timer_type::context_type>(metrics->timers.apply([&](std::map<std::string, metrics_t::timers_t>& timers)
        -> timer_type::context_type
    {
        auto& timer = cpp17::map::try_emplace(
            &timers,
            payload.event.name,
            context.metrics(),
            manifest().name,
            payload.event.name
        ).first->second.overall;

        return timer.context();
    }));

    auto self = shared_from_this();
    slave.inject(payload, [=](std::uint64_t) mutable {
        // TODO: Hack, but at least it saves from the deadlock.
        // TODO: Notify watcher about channel finish.
        observer.reset();
        loop->post(std::bind(&overseer_t::rebalance_events, self));
    });
    // TODO: Notify watcher about channel started.
}

void
overseer_t::despawn(const std::string& id, despawn_policy_t policy) {
    pool.apply([&](pool_type& pool) {
        auto it = pool.find(id);
        if (it != pool.end()) {
            switch (policy) {
            case despawn_policy_t::graceful:
                it->second.seal();
                break;
            case despawn_policy_t::force:
                pool.erase(it);
                // TODO: Notify watcher about slave death.
                loop->post(std::bind(&overseer_t::rebalance_slaves, shared_from_this()));
                break;
            default:
                BOOST_ASSERT(false);
            }
        }
    });
}

void
overseer_t::cancel() {
    COCAINE_LOG_DEBUG(log, "overseer is processing terminate request");

    keep_alive(0);
    pool->clear();
}

std::shared_ptr<control_t>
overseer_t::on_handshake(const std::string& id,
                         std::shared_ptr<session_t> session,
                         upstream<io::worker::control_tag>&& stream)
{
    blackhole::scoped_attributes_t holder(*log, {{ "uuid", id }});

    COCAINE_LOG_DEBUG(log, "processing handshake message");

    auto control = pool.apply([&](pool_type& pool) -> std::shared_ptr<control_t> {
        auto it = pool.find(id);
        if (it == pool.end()) {
            COCAINE_LOG_DEBUG(log, "rejecting slave as unexpected");
            return nullptr;
        }

        COCAINE_LOG_DEBUG(log, "activating slave");
        try {
            return it->second.activate(std::move(session), std::move(stream));
        } catch (const std::exception& err) {
            // The slave can be in invalid state; broken, for example, or because the overseer is
            // overloaded. In fact I hope it never happens.
            // Also unlikely we can receive here std::bad_alloc if unable to allocate more memory
            // for control dispatch.
            // If this happens the session will be closed.
            COCAINE_LOG_ERROR(log, "failed to activate the slave: %s", err.what());
        }

        return nullptr;
    });

    if (control) {
        // TODO: Notify watcher about slave spawn.
        loop->post(std::bind(&overseer_t::rebalance_events, shared_from_this()));
    }

    return control;
}

void
overseer_t::on_slave_death(const std::error_code& ec, std::string uuid) {
    if (ec) {
        COCAINE_LOG_DEBUG(log, "slave has removed itself from the pool: %s", ec.message());
        metrics->slaves.crashed.inc();
    } else {
        COCAINE_LOG_DEBUG(log, "slave has removed itself from the pool");
    }

    pool.apply([&](pool_type& pool) {
        auto it = pool.find(uuid);
        if (it != pool.end()) {
            it->second.terminate(ec);
            pool.erase(it);
        }
    });

    // TODO: Notify watcher about slave death.
    loop->post(std::bind(&overseer_t::rebalance_slaves, shared_from_this()));
}

void
overseer_t::rebalance_events() {
    using boost::adaptors::filtered;

    const auto concurrency = profile().concurrency;

    pool.apply([&](pool_type& pool) {
        if (pool.empty()) {
            return;
        }

        COCAINE_LOG_DEBUG(log, "rebalancing events queue");
        queue.apply([&](queue_type& queue) {
            while (!queue.empty()) {
                // Find an active slave with minimal load, which is not overloaded.
                const auto filter = std::function<bool(const slave_t&)>([&](const slave_t& slave) -> bool {
                    return slave.active() && slave.load() < concurrency;
                });

                const auto range = pool | boost::adaptors::map_values | filtered(filter);

                auto slave = boost::min_element(range, +[](const slave_t& lhs, const slave_t& rhs) -> bool {
                    return lhs.load() < rhs.load();
                });

                // No free slaves found.
                if(slave == boost::end(range)) {
                    return;
                }

                auto& payload = queue.front();

                try {
                    trace_t::restore_scope_t scope(payload.trace);
                    assign(*slave, *payload);
                    // The slave may become invalid and reject the assignment (or reject for any
                    // other reasons). We pop the channel only on successful assignment to achieve
                    // strong exception guarantee.
                    queue.pop_front();
                } catch (const std::exception& err) {
                    COCAINE_LOG_DEBUG(log, "slave has rejected assignment: %s", err.what());
                }
            }
        });
    });
}

void
overseer_t::rebalance_slaves() {
    using boost::adaptors::filtered;

    const auto load = queue->size();
    const auto profile = this->profile();

    const auto pool_target = this->pool_target.load();

    // Bound current pool target between [1; limit].
    const auto target = detail::bound(
        1UL,
        pool_target ? pool_target : load / profile.grow_threshold,
        profile.pool_limit
    );

    pool.apply([&](pool_type& pool) {
        if (pool_target) {
            COCAINE_LOG_DEBUG(log, "attempting to rebalance slaves using direct policy")(
                "load", load, "slaves", pool.size(), "target", target
            );

            if (target <= pool.size()) {
                unsigned long active = boost::count_if(pool | boost::adaptors::map_values, +[](const slave_t& slave) -> bool {
                    return slave.active();
                });

                COCAINE_LOG_DEBUG(log, "sealing up to %d active slaves", active);

                while (active-- > target) {
                    // Find active slave with minimal load.
                    const auto range = pool | boost::adaptors::map_values | filtered(+[](const slave_t& slave) -> bool {
                        return slave.active();
                    });

                    auto slave = boost::min_element(range, +[](const slave_t& lhs, const slave_t& rhs) -> bool {
                        return lhs.load() < rhs.load();
                    });

                    // All slaves are now inactive by some external conditions.
                    if (slave == boost::end(range)) {
                        break;
                    }

                    try {
                        COCAINE_LOG_DEBUG(log, "sealing slave")("uuid", slave->id());
                        slave->seal();
                    } catch (const std::exception& err) {
                        COCAINE_LOG_WARNING(log, "unable to seal slave: %s", err.what());
                    }
                }
            } else {
                while(pool.size() < target) {
                    spawn(pool);
                }
            }
        } else {
            COCAINE_LOG_DEBUG(log, "attempting to rebalance slaves using automatic policy")(
                "load", load, "slaves", pool.size(), "target", target
            );

            if (pool.size() >= profile.pool_limit || pool.size() * profile.grow_threshold >= load) {
                return;
            }

            while(pool.size() < target) {
                spawn(pool);
            }
        }
    });
}
