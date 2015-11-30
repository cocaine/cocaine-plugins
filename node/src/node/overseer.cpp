#include "cocaine/service/node/overseer.hpp"

#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>

#include <blackhole/scoped.hpp>

#include <cocaine/context.hpp>

#include "cocaine/api/stream.hpp"

#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"

#include "cocaine/detail/service/node/dispatch/client.hpp"
#include "cocaine/detail/service/node/dispatch/handshake.hpp"
#include "cocaine/detail/service/node/dispatch/worker.hpp"
#include "cocaine/detail/service/node/slave/control.hpp"
#include "cocaine/detail/service/node/slot.hpp"
#include "cocaine/detail/service/node/util.hpp"

#include <boost/accumulators/statistics/extended_p_square.hpp>

namespace ph = std::placeholders;

using namespace cocaine;

struct collector_t {
    std::size_t active;
    std::size_t cumload;

    explicit
    collector_t(const overseer_t::pool_type& pool):
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
    stats{}
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

    std::uint64_t spawned;
    std::uint64_t crashed;

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
            pinfo["total:spawned"] = value.spawned;
            pinfo["total:crashed"] = value.crashed;

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
    visitor.visit(stats.requests.accepted.load(), stats.requests.rejected.load());
    visitor.visit({ profile().queue_limit, &queue });
    visitor.visit(stats.quantiles());
    visitor.visit({ profile().pool_limit, stats.slaves.spawned, stats.slaves.crashed, &pool });

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

    auto write(const std::string& chunk) -> stream_t& {
        dispatch->stream().write(chunk);
        return *this;
    }

    auto error(const std::error_code& ec, const std::string& reason) -> void {
        dispatch->stream().abort(ec, reason);
    }

    auto close() -> void {
        dispatch->stream().close();
    }
};

struct rx_stream_t : public api::stream_t {
    typedef io::event_traits<io::worker::rpc::invoke>::upstream_type tag;
    typedef io::protocol<tag>::scope protocol;

    io::streaming_slot<io::app::enqueue>::upstream_type stream;

    rx_stream_t(io::streaming_slot<io::app::enqueue>::upstream_type stream) :
        stream(std::move(stream))
    {}

    auto write(const std::string& chunk) -> stream_t& {
        stream = stream.send<protocol::chunk>(chunk);
        return *this;
    }

    auto error(const std::error_code& ec, const std::string& reason) -> void {
        stream.send<protocol::error>(ec, reason);
    }

    auto close() -> void {
        stream.send<protocol::choke>();
    }
};

} // namespace

std::shared_ptr<client_rpc_dispatch_t>
overseer_t::enqueue(io::streaming_slot<io::app::enqueue>::upstream_type downstream,
                    app::event_t event,
                    boost::optional<service::node::slave::id_t> id)
{
    std::shared_ptr<api::stream_t> rx = std::make_shared<rx_stream_t>(std::move(downstream));
    auto tx = enqueue(rx, std::move(event), std::move(id));
    return std::static_pointer_cast<tx_stream_t>(tx)->dispatch;
}

auto overseer_t::enqueue(std::shared_ptr<api::stream_t> rx,
    app::event_t event,
    boost::optional<service::node::slave::id_t> /*id*/) -> std::shared_ptr<api::stream_t>
{
    auto tx = std::make_shared<tx_stream_t>();

    count_success_if(&stats.requests.accepted, &stats.requests.rejected, [&] {
        queue.apply([&](queue_type& queue) {
            const auto limit = profile().queue_limit;

            if (queue.size() >= limit && limit > 0) {
                throw std::system_error(error::queue_is_full);
            }

            tx->dispatch = std::make_shared<client_rpc_dispatch_t>(manifest().name);

            queue.push_back({
                {std::move(event), tx->dispatch, std::move(rx)},
                trace_t::current()
            });
        });
    });

    rebalance_events();
    rebalance_slaves();

    return tx;
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
    COCAINE_LOG_INFO(log, "enlarging the slaves pool to {}", pool.size() + 1);

    slave_context ctx(context, manifest(), profile());

    // It is guaranteed that the cleanup handler will not be invoked from within the slave's
    // constructor.
    const auto uuid = ctx.id;
    pool.insert(std::make_pair(
        uuid,
        slave_t(std::move(ctx), *loop, std::bind(&overseer_t::on_slave_death, shared_from_this(), ph::_1, uuid))
    ));

    ++stats.slaves.spawned;
}

void
overseer_t::assign(slave_t& slave, slave::channel_t& payload) {
    // Attempts to inject the new channel into the slave.
    const auto id = slave.id();
    const auto timestamp = payload.event.birthstamp;

    // TODO: Race possible.
    auto self = shared_from_this();
    slave.inject(payload, [=](std::uint64_t) {
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
    auto scoped = log->scoped({{ "uuid", id }});

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
        loop->post(std::bind(&overseer_t::rebalance_events, shared_from_this()));
    }

    return control;
}

void
overseer_t::on_slave_death(const std::error_code& ec, std::string uuid) {
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
                    COCAINE_LOG_DEBUG(log, "slave has rejected assignment: {}", err.what());
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
            COCAINE_LOG_DEBUG(log, "attempting to rebalance slaves using direct policy", {
                {"load", load}, {"slaves", pool.size()}, {"target", target}
            });

            if (target <= pool.size()) {
                unsigned long active = boost::count_if(pool | boost::adaptors::map_values, +[](const slave_t& slave) -> bool {
                    return slave.active();
                });

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
