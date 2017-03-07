#include "cocaine/detail/service/node/engine.hpp"

#include <boost/accumulators/statistics/extended_p_square.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>

#include <blackhole/logger.hpp>
#include <blackhole/scope/holder.hpp>

#include <metrics/registry.hpp>

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

#include "cocaine/api/authentication.hpp"
#include "cocaine/api/stream.hpp"

#include "cocaine/service/node/manifest.hpp"
#include "cocaine/service/node/profile.hpp"
#include "cocaine/service/node/slave/error.hpp"
#include "cocaine/service/node/slave/id.hpp"

#include "cocaine/detail/service/node/dispatch/client.hpp"
#include "cocaine/detail/service/node/dispatch/handshake.hpp"
#include "cocaine/detail/service/node/dispatch/worker.hpp"
#include "cocaine/detail/service/node/slave/control.hpp"
#include "cocaine/detail/service/node/slave/stats.hpp"

#include "pool_observer.hpp"
#include "util/bound.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {

namespace ph = std::placeholders;

struct collector_t {
    std::size_t active;
    std::size_t cumload;

    explicit
    collector_t(const engine_t::pool_type& pool):
        active{},
        cumload{}
    {
        for (const auto& it : pool) {
            const auto load = it.second.load();
            if (it.second.active() && load) {
                active++;
                cumload += load;
            }
        }
    }
};

engine_t::engine_t(context_t& context,
                   manifest_t manifest,
                   profile_t profile,
                   pool_observer& observer,
                   std::shared_ptr<asio::io_service> loop):
    log(context.log(format("{}/overseer", manifest.name))),
    context(context),
    stopped(false),
    birthstamp(std::chrono::system_clock::now()),
    manifest_(std::move(manifest)),
    profile_(profile),
    auth(api::authentication(context, "core", manifest_.name)),
    loop(loop),
    pool_target{},
    last_timeout(std::chrono::seconds(1)),
    observer(observer),
    stats(context, manifest_.name, std::chrono::seconds(2))
{
    COCAINE_LOG_DEBUG(log, "overseer has been initialized");
}

engine_t::~engine_t() {
    COCAINE_LOG_DEBUG(log, "overseer has been destroyed");
}

auto engine_t::active_workers() const -> std::uint32_t {
    return pool.apply([&](const pool_type& pool) -> std::uint32_t {
        std::uint32_t active = 0;
        for (const auto& kv : pool) {
            if (kv.second.active()) {
                ++active;
            }
        }
        return active;
    });
}

manifest_t
engine_t::manifest() const {
    return manifest_;
}

profile_t
engine_t::profile() const {
    return *profile_.synchronize();
}

namespace {

// Helper tagged struct.
struct queue_t {
    unsigned long capacity;

    const synchronized<engine_t::queue_type>* queue;
    metrics::usts::ewma_t& queue_depth;
};

// Helper tagged struct.
struct pool_t {
    unsigned long capacity;

    std::int64_t spawned;
    std::int64_t crashed;

    const synchronized<engine_t::pool_type>* pool;
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
    visit(std::int64_t accepted, std::int64_t rejected) {
        dynamic_t::object_t info;

        info["accepted"] = accepted;
        info["rejected"] = rejected;

        result["requests"] = info;
    }

    // Pending events queue.
    void
    visit(const queue_t& value) {
        dynamic_t::object_t info;

        info["capacity"] = value.capacity;

        const auto now = std::chrono::high_resolution_clock::now();

        value.queue->apply([&](const engine_t::queue_type& queue) {
            info["depth"] = queue.size();

            // Wake up aggregator.
            value.queue_depth.add(queue.size());
            info["depth_average"] = trunc(value.queue_depth.get(), 3);

            typedef engine_t::queue_type::value_type value_type;

            if (queue.empty()) {
                info["oldest_event_age"] = 0;
            } else {
                const auto min = *boost::min_element(queue |
                    boost::adaptors::transformed(+[](const value_type& cur) {
                        return cur.event.birthstamp;
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
    visit(metrics::meter_t& meter) {
        dynamic_t::array_t info {{
            trunc(meter.m01rate(), 3),
            trunc(meter.m05rate(), 3),
            trunc(meter.m15rate(), 3)
        }};

        result["rate"] = info;
        result["rate_mean"] = trunc(meter.mean_rate(), 3);
    }

    inline
    double
    trunc(double v, uint n) noexcept {
        BOOST_ASSERT(n < 10);

        static const long long table[10] = {
            1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000
        };

        return std::ceil(v * table[n]) / table[n];
    }

    template<typename Accumulate>
    void
    visit(metrics::timer<Accumulate>& timer) {
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

        const auto find = [&](const double ms) -> double {
            return 100.0 * snapshot.phi(ms);
        };

        reversed["1ms"]    = trunc(find(1e6), 2);
        reversed["2ms"]    = trunc(find(2e6), 2);
        reversed["5ms"]    = trunc(find(5e6), 2);
        reversed["10ms"]   = trunc(find(10e6), 2);
        reversed["20ms"]   = trunc(find(20e6), 2);
        reversed["50ms"]   = trunc(find(50e6), 2);
        reversed["100ms"]  = trunc(find(100e6), 2);
        reversed["500ms"]  = trunc(find(500e6), 2);
        reversed["1000ms"] = trunc(find(1000e6), 2);

        result["timings_reversed"] = reversed;
    }

    void
    visit(const pool_t& value) {
        const auto now = std::chrono::high_resolution_clock::now();

        value.pool->apply([&](const engine_t::pool_type& pool) {
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
            pinfo["total:spawned"] = value.spawned;
            pinfo["total:crashed"] = value.crashed;

            result["pool"] = pinfo;
        });
    }
};

} // namespace

auto engine_t::info(io::node::info::flags_t flags) const -> dynamic_t::object_t {
    dynamic_t::object_t result;

    result["uptime"] = uptime().count();

    info_visitor_t visitor(flags, &result);
    auto profile = this->profile();
    visitor.visit(manifest());
    visitor.visit(profile);
    visitor.visit(stats.requests.accepted->load(), stats.requests.rejected->load());
    visitor.visit({profile.queue_limit, &queue, *stats.queue_depth});
    visitor.visit(*stats.meter.get());
    visitor.visit(*stats.timer.get());
    visitor.visit({profile.pool_limit, stats.slaves.spawned->load(), stats.slaves.crashed->load(), &pool});

    return result;
}

auto engine_t::uptime() const -> std::chrono::seconds {
    const auto now = std::chrono::system_clock::now();

    return std::chrono::duration_cast<std::chrono::seconds>(now - birthstamp);
}

auto engine_t::control_population(int count) -> void {
    count = std::max(0, count);
    COCAINE_LOG_DEBUG(log, "changed keep-alive slave count to {}", count);

    pool_target = count;
    rebalance_slaves();
}

namespace {

struct tx_stream_t : public api::stream_t {
    std::shared_ptr<client_rpc_dispatch_t> dispatch;

    auto write(hpack::header_storage_t headers, const std::string& chunk) -> stream_t& {
        dispatch->write(std::move(headers), chunk);
        return *this;
    }

    auto error(hpack::header_storage_t headers, const std::error_code& ec, const std::string& reason) -> void {
        dispatch->abort(std::move(headers), ec, reason);
    }

    auto close(hpack::header_storage_t headers) -> void {
        dispatch->close(std::move(headers));
    }
};

struct rx_stream_t : public api::stream_t {
    typedef io::event_traits<io::worker::rpc::invoke>::upstream_type tag;
    typedef io::protocol<tag>::scope protocol;

    upstream<io::stream_of<std::string>::tag> stream;

    rx_stream_t(upstream<io::stream_of<std::string>::tag> stream) :
        stream(std::move(stream))
    {}

    auto write(hpack::header_storage_t headers, const std::string& chunk) -> stream_t& {
        stream = stream.send<protocol::chunk>(std::move(headers), chunk);
        return *this;
    }

    auto error(hpack::header_storage_t headers, const std::error_code& ec, const std::string& reason) -> void {
        stream.send<protocol::error>(std::move(headers), ec, reason);
    }

    auto close(hpack::header_storage_t headers) -> void {
        stream.send<protocol::choke>(std::move(headers));
    }
};

} // namespace

auto engine_t::enqueue(upstream<io::stream_of<std::string>::tag> downstream,
                       event_t event,
                       boost::optional<id_t> id) -> std::shared_ptr<client_rpc_dispatch_t>
{
    const std::shared_ptr<api::stream_t> rx = std::make_shared<rx_stream_t>(std::move(downstream));
    const auto tx = enqueue(std::move(rx), std::move(event), std::move(id));
    return std::static_pointer_cast<tx_stream_t>(tx)->dispatch;
}

auto engine_t::enqueue(std::shared_ptr<api::stream_t> rx,
                       event_t event,
                       boost::optional<id_t> id) -> std::shared_ptr<api::stream_t>
{
    auto tx = std::make_shared<tx_stream_t>();

    try {
        queue.apply([&](queue_type& queue) {
            const auto limit = profile().queue_limit;

            if (queue.size() >= limit && limit > 0) {
                throw std::system_error(error::queue_is_full);
            }

            if (id) {
                pool.apply([&](pool_type& pool) {
                    if (pool.count(id->id()) == 0) {
                        spawn(*id, pool);
                    }
                });
            }

            tx->dispatch = std::make_shared<client_rpc_dispatch_t>(manifest().name);

            queue.push_back({
                std::move(event),
                trace_t::current(),
                std::move(id),
                tx->dispatch, // Explicitly copy.
                std::move(rx)
            });
            stats.queue_depth->add(queue.size());
        });

        stats.requests.accepted->fetch_add(1);
        stats.meter->mark();
        rebalance_events();
        rebalance_slaves();
    } catch (...) {
        stats.requests.rejected->fetch_add(1);
        rebalance_events();
        rebalance_slaves();
        throw;
    }

    return tx;
}

auto engine_t::prototype() -> io::dispatch_ptr_t {
    return std::make_shared<const handshaking_t>(
        manifest().name,
        std::bind(&engine_t::on_handshake, shared_from_this(), ph::_1, ph::_2, ph::_3)
    );
}

auto engine_t::spawn(pool_type& pool) -> void {
    spawn(id_t(), pool);
}

auto engine_t::spawn(const id_t& id, pool_type& pool) -> void {
    const auto profile = this->profile();
    if (pool.size() >= profile.pool_limit) {
        throw std::system_error(error::pool_is_full, "the pool is full");
    }

    COCAINE_LOG_INFO(log, "enlarging the slaves pool to {}", pool.size() + 1);

    // It is guaranteed that the cleanup handler will not be invoked from within the slave's
    // constructor.
    pool.insert(std::make_pair(
        id.id(),
        slave_t(
            context,
            id,
            manifest(),
            profile,
            auth,
            *loop,
            std::bind(&engine_t::on_slave_death, shared_from_this(), ph::_1, id.id())
        )
    ));

    stats.slaves.spawned->fetch_add(1);
}

auto engine_t::assign(slave_t& slave, load_t& load) -> void {
    trace_t::restore_scope_t scope(load.trace);

    auto& event = load.event;

    std::chrono::milliseconds request_timeout(profile().request_timeout());
    if (auto timeout_from_header = hpack::header::convert_first<std::uint64_t>(event.headers, "request_timeout")) {
        request_timeout = std::chrono::milliseconds(*timeout_from_header);
    }

    if (event.birthstamp + request_timeout < std::chrono::high_resolution_clock::now()) {
        COCAINE_LOG_ERROR(log, "event {} has expired, dropping", event.name);
        load.downstream->error({}, error::deadline_error, "the event has expired in the queue");
        return;
    }

    // TODO: Drop due to replacement with header.
    if (load.event.deadline && *load.event.deadline < std::chrono::high_resolution_clock::now()) {
        COCAINE_LOG_DEBUG(log, "event has expired, dropping");

        load.downstream->error({}, error::deadline_error, "the event has expired in the queue");
        return;
    }

    auto self = shared_from_this();
    auto timer = std::make_shared<metrics::timer_t::context_t>(stats.timer->context());
    slave.inject(load, [this, self, timer](std::uint64_t) {
        // TODO: Hack, but at least it saves from the deadlock.
        loop->post(std::bind(&engine_t::rebalance_events, self));
    });
}

auto engine_t::despawn(const std::string& id, despawn_policy_t policy) -> void {
    pool.apply([&](pool_type& pool) {
        auto it = pool.find(id);
        if (it != pool.end()) {
            switch (policy) {
            case despawn_policy_t::graceful:
                it->second.seal();
                break;
            case despawn_policy_t::force:
                pool.erase(it);
                observer.despawned();
                loop->post(std::bind(&engine_t::rebalance_slaves, shared_from_this()));
                break;
            default:
                BOOST_ASSERT(false);
            }
        }
    });
}

auto engine_t::on_handshake(const std::string& id, std::shared_ptr<session_t> session,
                            upstream<io::worker::control_tag>&& stream)
    -> std::shared_ptr<control_t>
{
    const blackhole::scope::holder_t scoped(*log, {{ "uuid", id }});

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
            COCAINE_LOG_ERROR(log, "failed to activate the slave: {}", err.what());
        }

        return nullptr;
    });

    if (control) {
        observer.spawned();
        loop->post(std::bind(&engine_t::rebalance_events, shared_from_this()));
    }

    return control;
}

auto engine_t::on_slave_death(const std::error_code& ec, std::string uuid) -> void {
    if (ec) {
        on_spawn_rate_timer.apply([&](std::unique_ptr<asio::deadline_timer>& timer) {
            if (timer) {
                // We already have fallback timer in progress.
                // TODO: Increment stats.slaves.timeouted.
            } else {
                timer.reset(new asio::deadline_timer(*loop));
                if (std::chrono::system_clock::now() - last_failed < std::chrono::seconds(32)) { // TODO: Magic.
                    last_timeout = std::chrono::seconds(
                        std::min(static_cast<long long int>(last_timeout.count() * 2), 32LL)
                    ); // TODO: Magic.
                } else {
                    last_timeout = std::chrono::seconds(1); // TODO: Magic.
                }

                timer->expires_from_now(boost::posix_time::seconds(last_timeout.count()));
                timer->async_wait(std::bind(&engine_t::on_spawn_rate_timeout, shared_from_this(), ph::_1));

                COCAINE_LOG_INFO(log, "next rebalance occurs in {} s", last_timeout.count());
            }

            last_failed = std::chrono::system_clock::now();
        });

        COCAINE_LOG_DEBUG(log, "slave has removed itself from the pool: {}", ec.message());
        stats.slaves.crashed->fetch_add(1);
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

    observer.despawned();
    loop->post(std::bind(&engine_t::rebalance_slaves, shared_from_this()));
}

auto engine_t::on_spawn_rate_timeout(const std::error_code&) -> void {
    on_spawn_rate_timer.apply([&](std::unique_ptr<asio::deadline_timer>& timer) {
        timer.reset();
    });

    COCAINE_LOG_INFO(log, "rebalancing slaves after exponential backoff timeout");
    rebalance_slaves();
}

auto engine_t::rebalance_events() -> void {
    using boost::adaptors::filtered;

    const auto concurrency = profile().concurrency;

    // Find an active non-overloaded slave.
    const auto filter = std::function<bool(const slave_t&)>([&](const slave_t& slave) -> bool {
        return slave.active() && slave.load() < concurrency;
    });

    pool.apply([&](pool_type& pool) {
        if (pool.empty()) {
            return;
        }

        COCAINE_LOG_DEBUG(log, "rebalancing events queue");
        queue.apply([&](queue_type& queue) {
            while (!queue.empty()) {
                auto& load = queue.front();

                COCAINE_LOG_DEBUG(log, "rebalancing event");

                // If we are dealing with tagged events we need to find an active slave with the
                // specified id.
                if (load.id && pool.count(load.id->id()) != 0) {
                    auto& slave = pool.at(load.id->id());

                    if (filter(slave)) {
                        // In case of tagged events the rejected event is an error, which can only
                        // occur in the case of state (slave has died), out of the memory issues
                        // or something else out of out control.
                        // The only reasonable way is to notify the client about the error allowing
                        // it to retry if required.
                        try {
                            assign(slave, load);
                        } catch (const std::exception& err) {
                            COCAINE_LOG_WARNING(log, "slave has rejected assignment: {}", err.what());

                            try {
                                load.downstream->error({}, error::invalid_assignment, err.what());
                            } catch (const std::exception& err) {
                                COCAINE_LOG_WARNING(log, "failed to notify assignment failure: {}", err.what());
                            }
                        }
                        queue.pop_front();
                        stats.queue_depth->add(queue.size());
                    }
                } else {
                    const auto range = pool | boost::adaptors::map_values | filtered(filter);

                    // Here we try to find an active non-overloaded slave with minimal load.
                    auto slave = boost::min_element(
                        range, +[](const slave_t& lhs, const slave_t& rhs) -> bool {
                            return lhs.load() < rhs.load();
                        });

                    // No free slaves found.
                    if (slave == boost::end(range)) {
                        COCAINE_LOG_DEBUG(log, "no free slaves found, rebalancing is over");
                        return;
                    }

                    try {
                        assign(*slave, load);
                        // The slave may become invalid and reject the assignment or reject for any
                        // other reasons. We pop the channel only on successful assignment to
                        // achieve strong exception guarantee.
                        queue.pop_front();
                        stats.queue_depth->add(queue.size());
                    } catch (const std::exception& err) {
                        COCAINE_LOG_WARNING(log, "slave has rejected assignment: {}", err.what());
                        loop->post([&] {
                            rebalance_slaves();
                        });
                        return;
                    }
                }
            }
        });
    });
}

auto engine_t::rebalance_slaves() -> void {
    if (stopped) {
        return;
    }

    using boost::adaptors::filtered;

    const auto load = queue->size();
    const auto profile = this->profile();

    const auto pool_target = static_cast<std::size_t>(this->pool_target.load());

    // Bound current pool target between [1; limit].
    const auto target = util::bound(
        1ul,
        pool_target ? pool_target : load / profile.grow_threshold,
        profile.pool_limit
    );

    if (*on_spawn_rate_timer.synchronize()) {
        return;
    }

    pool.apply([&](pool_type& pool) {
        if (pool_target) {
            COCAINE_LOG_DEBUG(log, "attempting to rebalance slaves using direct policy", {
                {"load", load},
                {"slaves", pool.size()},
                {"target", target}
            });

            if (target <= pool.size()) {
                auto active = static_cast<std::size_t>(boost::count_if(pool | boost::adaptors::map_values, +[](const slave_t& slave) -> bool {
                    return slave.active();
                }));

                COCAINE_LOG_DEBUG(log, "sealing up to {} active slaves", active);

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
                        COCAINE_LOG_DEBUG(log, "sealing slave", {{"uuid", slave->id()}});
                        slave->seal();
                    } catch (const std::exception& err) {
                        COCAINE_LOG_WARNING(log, "unable to seal slave: {}", err.what());
                    }
                }
            } else {
                while(pool.size() < target) {
                    spawn(pool);
                }
            }
        } else {
            COCAINE_LOG_DEBUG(log, "attempting to rebalance slaves using automatic policy", {
                {"load", load}, {"slaves", pool.size()}, {"target", target}
            });

            if (pool.size() >= profile.pool_limit || pool.size() * profile.grow_threshold >= load) {
                return;
            }

            while(pool.size() < target) {
                spawn(pool);
            }
        }
    });
}

} // namespace node
} // namespace service
} // namespace detail
} // namespace cocaine
