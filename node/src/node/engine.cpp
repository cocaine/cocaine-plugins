#include "cocaine/detail/service/node/engine.hpp"

#include <boost/accumulators/statistics/extended_p_square.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>

#include <blackhole/logger.hpp>
#include <blackhole/scope/holder.hpp>

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

#include "cocaine/api/stream.hpp"

#include "cocaine/service/node/manifest.hpp"
#include "cocaine/service/node/profile.hpp"
#include "cocaine/service/node/slave/error.hpp"
#include "cocaine/service/node/slave/id.hpp"

#include "cocaine/detail/service/node/dispatch/client.hpp"
#include "cocaine/detail/service/node/dispatch/handshake.hpp"
#include "cocaine/detail/service/node/dispatch/worker.hpp"
#include "cocaine/detail/service/node/rpc/slot.hpp"
#include "cocaine/detail/service/node/slave/control.hpp"
#include "cocaine/detail/service/node/slave/stats.hpp"
#include "cocaine/detail/service/node/util.hpp"

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
                       std::shared_ptr<asio::io_service> loop):
    log(context.log(format("{}/overseer", manifest.name))),
    context(context),
    birthstamp(std::chrono::system_clock::now()),
    manifest_(std::move(manifest)),
    profile_(profile),
    loop(loop),
    pool_target{},
    stats{}
{
    COCAINE_LOG_DEBUG(log, "overseer has been initialized");
}

engine_t::~engine_t() {
    COCAINE_LOG_DEBUG(log, "overseer has been destroyed");
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
};

// Helper tagged struct.
struct pool_t {
    unsigned long capacity;

    std::uint64_t spawned;
    std::uint64_t crashed;

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
    visit(std::uint64_t accepted, std::uint64_t rejected) {
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

    // Response time quantiles over all events.
    void
    visit(const std::vector<stats_t::quantile_t>& value) {
        dynamic_t::object_t info;

        std::array<char, 16> buf;
        for (const auto& quantile : value) {
            if (std::snprintf(buf.data(), buf.size(), "%.2f%%", quantile.probability)) {
                info[buf.data()] = quantile.value;
            }
        }

        result["timings"] = info;
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
    auto _profile = profile();
    visitor.visit(manifest());
    visitor.visit(_profile);
    visitor.visit(stats.requests.accepted.load(), stats.requests.rejected.load());
    visitor.visit({ _profile.queue_limit, &queue });
    visitor.visit(stats.quantiles());
    visitor.visit({ _profile.pool_limit, stats.slaves.spawned, stats.slaves.crashed, &pool });

    return result;
}

auto engine_t::uptime() const -> std::chrono::seconds {
    const auto now = std::chrono::system_clock::now();

    return std::chrono::duration_cast<std::chrono::seconds>(now - birthstamp);
}

auto engine_t::failover(int count) -> void {
    count = std::max(0, count);
    COCAINE_LOG_DEBUG(log, "changed keep-alive slave count to {}", count);

    pool_target = count;
    rebalance_slaves();
}

namespace {

template<typename SuccCounter, typename FailCounter, typename F>
void count_success_if(SuccCounter* succ, FailCounter* fail, F fn) {
    try {
        fn();
        ++(*succ);
    } catch (...) {
        ++(*fail);
        throw;
    }
}

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

    io::streaming_slot<io::app::enqueue>::upstream_type stream;

    rx_stream_t(io::streaming_slot<io::app::enqueue>::upstream_type stream) :
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

auto engine_t::enqueue(upstream<io::stream_of<std::string>::tag> downstream, event_t event,
                       boost::optional<id_t> id) -> std::shared_ptr<client_rpc_dispatch_t> {
    std::shared_ptr<api::stream_t> rx = std::make_shared<rx_stream_t>(std::move(downstream));
    auto tx = enqueue(rx, std::move(event), std::move(id));
    return std::static_pointer_cast<tx_stream_t>(tx)->dispatch;
}

auto engine_t::enqueue(std::shared_ptr<api::stream_t> rx,
    event_t event,
    boost::optional<id_t> id) -> std::shared_ptr<api::stream_t>
{
    auto tx = std::make_shared<tx_stream_t>();

    count_success_if(&stats.requests.accepted, &stats.requests.rejected, [&] {
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

            queue.push_back({std::move(event), trace_t::current(), id, tx->dispatch, std::move(rx)});
        });
    });

    rebalance_events();
    rebalance_slaves();

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
    auto _profile = profile();
    if (pool.size() >= _profile.pool_limit) {
        throw std::system_error(error::pool_is_full, "the pool is full");
    }

    COCAINE_LOG_INFO(log, "enlarging the slaves pool to {}", pool.size() + 1);

    // It is guaranteed that the cleanup handler will not be invoked from within the slave's
    // constructor.
    pool.insert(std::make_pair(
        id.id(),
        slave_t(context, id, manifest(), _profile, *loop, std::bind(&engine_t::on_slave_death, shared_from_this(), ph::_1, id.id()))
    ));

    ++stats.slaves.spawned;
}

auto engine_t::assign(slave_t& slave, load_t& load) -> void {
    trace_t::restore_scope_t scope(load.trace);
    if (load.event.deadline && *load.event.deadline < std::chrono::high_resolution_clock::now()) {
        COCAINE_LOG_DEBUG(log, "event has expired, dropping");

        load.downstream->error({}, error::deadline_error, "the event has expired in the queue");
        return;
    }

    // Attempts to inject the new channel into the slave.
    const auto id = slave.id();
    const auto timestamp = load.event.birthstamp;

    // TODO: Race possible.
    auto self = shared_from_this();
    slave.inject(load, [=](std::uint64_t) {
        const auto now = std::chrono::high_resolution_clock::now();
        const auto elapsed = std::chrono::duration<
            double,
            std::chrono::milliseconds::period
        >(now - timestamp).count();

        stats.timings.apply([&](stats_t::quantiles_t& timings) {
            timings(elapsed);
        });

        // TODO: Hack, but at least it saves from the deadlock.
        // TODO: Notify watcher about channel finish.
        loop->post(std::bind(&engine_t::rebalance_events, self));
    });

    // TODO: Notify watcher about channel started.
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
                // TODO: Notify watcher about slave death.
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
    -> std::shared_ptr<control_t> {
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
        // TODO: Notify watcher about slave spawn.
        loop->post(std::bind(&engine_t::rebalance_events, shared_from_this()));
    }

    return control;
}

auto engine_t::on_slave_death(const std::error_code& ec, std::string uuid) -> void {
    if (ec) {
        COCAINE_LOG_DEBUG(log, "slave has removed itself from the pool: {}", ec.message());
        ++stats.slaves.crashed;
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
    loop->post(std::bind(&engine_t::rebalance_slaves, shared_from_this()));
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
                        return;
                    }

                    try {
                        assign(*slave, load);
                        // The slave may become invalid and reject the assignment or reject for any
                        // other reasons. We pop the channel only on successful assignment to
                        // achieve strong exception guarantee.
                        queue.pop_front();
                    } catch (const std::exception& err) {
                        COCAINE_LOG_WARNING(log, "slave has rejected assignment: {}", err.what());
                    }
                }
            }
        });
    });
}

auto engine_t::rebalance_slaves() -> void {
    using boost::adaptors::filtered;

    const auto load = queue->size();
    const auto profile = this->profile();

    const auto pool_target = static_cast<std::size_t>(this->pool_target.load());

    // Bound current pool target between [1; limit].
    const auto target = detail::bound(
        1UL,
        pool_target ? pool_target : load / profile.grow_threshold,
        profile.pool_limit
    );

    pool.apply([&](pool_type& pool) {
        if (pool_target) {
            COCAINE_LOG_DEBUG(log, "attempting to rebalance slaves using direct policy", {
                {"load", load}, {"slaves", pool.size()}, {"target", target}
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
