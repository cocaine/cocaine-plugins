#include "engine.hpp"

#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/numeric.hpp>

#include <blackhole/logger.hpp>
#include <blackhole/scope/holder.hpp>

#include <cocaine/format.hpp>
#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/repository.hpp>

#include <metrics/factory.hpp>
#include <metrics/registry.hpp>

#include "cocaine/api/authentication.hpp"
#include "cocaine/api/isolate.hpp"
#include "cocaine/api/stream.hpp"

#include "cocaine/repository/isolate.hpp"

#include "cocaine/service/node/manifest.hpp"
#include "cocaine/service/node/profile.hpp"
#include "cocaine/service/node/slave/error.hpp"
#include "cocaine/service/node/slave/id.hpp"

#include "cocaine/detail/service/node/dispatch/client.hpp"
#include "cocaine/detail/service/node/dispatch/handshake.hpp"
#include "cocaine/detail/service/node/dispatch/worker.hpp"
#include "cocaine/detail/service/node/slave/control.hpp"

#include "isometrics.hpp"
#include "pool_observer.hpp"
#include "stdext/clamp.hpp"

#include "info/collector.hpp"
#include "info/manifest.hpp"
#include "info/profile.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {

namespace ph = std::placeholders;

engine_t::engine_t(context_t& context,
                   manifest_t manifest,
                   profile_t profile,
                   std::shared_ptr<pool_observer> observer,
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
    stats(context, manifest_.name, std::chrono::seconds(2))
{
    attach_pool_observer(std::move(observer));
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

auto engine_t::pooled_workers_ids() const -> std::vector<std::string> {
    using boost::adaptors::map_keys;

    std::vector<std::string> ids;
    return pool.apply([&](const pool_type& pool){
        ids.reserve(pool.size());
        boost::copy(pool | map_keys, std::back_inserter(ids));
        return ids;
    });
}

auto
engine_t::start_isolate_metrics_poll() -> void
{
    auto isolate = profile_.apply([&](const profile_t& profile) {
        return context.repository().get<api::isolate_t>(
        profile.isolate.type,
        context,
        *loop,
        manifest_.name,
        profile.isolate.type,
        profile.isolate.args);
    });

    metrics_retriever = metrics_retriever_t::make_and_ignite(
        context,
        manifest_.name,
        isolate,
        shared_from_this(),
        *loop,
        observers);
}

auto
engine_t::stop_isolate_metrics_poll() -> void
{
    metrics_retriever.reset();
}

manifest_t
engine_t::manifest() const {
    return manifest_;
}

profile_t
engine_t::profile() const {
    return *profile_.synchronize();
}

void
engine_t::attach_pool_observer(std::shared_ptr<pool_observer> observer) {
    observers->emplace_back(std::move(observer));
}

auto engine_t::info(io::node::info::flags_t flags) const -> dynamic_t::object_t {
    dynamic_t::object_t result;

    result["uptime"] = uptime().count();

    auto profile = this->profile();
    cocaine::service::node::info::manifest_t(manifest(), flags).apply(result);
    cocaine::service::node::info::profile_t(profile, flags).apply(result);

    cocaine::service::node::info::info_collector_t collector(flags, &result);
    collector.visit(stats.requests.accepted->load(), stats.requests.rejected->load());
    collector.visit({profile.queue_limit, &queue, *stats.queue_depth});
    collector.visit(*stats.meter.get());
    collector.visit(*stats.timer.get());
    collector.visit({profile.pool_limit, stats.slaves.spawned->load(), stats.slaves.crashed->load(), &pool});

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

auto engine_t::enqueue(event_t event, upstream<io::stream_of<std::string>::tag> downstream)
    -> std::shared_ptr<client_rpc_dispatch_t>
{
    const std::shared_ptr<api::stream_t> rx = std::make_shared<rx_stream_t>(std::move(downstream));
    auto tx = enqueue(std::move(event), std::move(rx));
    return std::static_pointer_cast<tx_stream_t>(std::move(tx))->dispatch;
}

auto engine_t::enqueue(event_t event, std::shared_ptr<api::stream_t> rx)
    -> std::shared_ptr<api::stream_t>
{
    stats.meter->mark();

    auto tx = std::make_shared<tx_stream_t>();

    const auto profile = this->profile();
    const auto limit = profile.queue_limit;

    try {
        pool.apply([&](pool_type& pool) {
            queue.apply([&](queue_type& queue) {
                if (limit > 0 && queue.size() >= limit) {
                    throw std::system_error(error::queue_is_full);
                }

                tx->dispatch = std::make_shared<client_rpc_dispatch_t>(manifest().name);
                auto load = load_t{
                    std::move(event),
                    trace_t::current(),
                    tx->dispatch, // Explicitly copy.
                    std::move(rx)
                };

                if (limit == 0) {
                    auto pressure = pool_pressure(pool);
                    auto vacant = profile.pool_limit * profile.concurrency - pressure;

                    if (queue.size() >= vacant) {
                        throw std::system_error(error::queue_is_full);
                    }
                }

                queue.push_back(load);
                stats.queue_depth->add(queue.size());

                stats.requests.accepted->fetch_add(1);
                rebalance_events(pool, queue);
            });
        });

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
    return std::make_shared<handshaking_t>(
        manifest().name,
        std::bind(&engine_t::on_handshake, shared_from_this(), ph::_1, ph::_2, ph::_3)
    );
}

auto engine_t::spawn(pool_type& pool) -> void {
    spawn(id_t(), pool);
}

auto engine_t::spawn(id_t id, pool_type& pool) -> void {
    const auto profile = this->profile();
    if (pool.size() >= profile.pool_limit) {
        throw std::system_error(error::pool_is_full, "the pool is full");
    }

    COCAINE_LOG_INFO(log, "enlarging the slaves pool to {}", pool.size() + 1);

    // It is guaranteed that the cleanup handler will not be invoked from within the slave's
    // constructor.
    pool.insert(std::make_pair(
        id.id(),
        slave_t(context, id, manifest(), profile, auth, *loop,
            std::bind(&engine_t::on_slave_death, shared_from_this(), ph::_1, id.id()))
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

    const auto drop_event = [&] {
        COCAINE_LOG_WARNING(log, "event {} has expired, dropping", event.name);
        try {
            load.downstream->error({}, error::deadline_error, "the event has expired in the queue");
        } catch (const std::system_error& err) {
            COCAINE_LOG_DEBUG(log, "failed to notify assignment failure: {}", error::to_string(err));
        }
    };

    if (event.birthstamp + request_timeout < std::chrono::high_resolution_clock::now()) {
        drop_event();
        return;
    }

    // TODO: Drop due to replacement with header.
    if (event.deadline && *event.deadline < std::chrono::high_resolution_clock::now()) {
        drop_event();
        return;
    }

    auto self = shared_from_this();
    auto timer = std::make_shared<metrics::timer_t::context_t>(stats.timer->context());
    slave.inject(load, [this, self, timer](std::uint64_t) {
        // TODO: Hack, but at least it saves from the deadlock.
        loop->post([&, self] {
            rebalance_events();
        });
    });
}

auto engine_t::despawn(const std::string& id, despawn_policy_t policy) -> void {

    const auto was_despawned = pool.apply([&](pool_type& pool) {
        auto it = pool.find(id);
        if (it != pool.end()) {
            switch (policy) {
            case despawn_policy_t::graceful:
                it->second.seal();
                break;
            case despawn_policy_t::force:
                pool.erase(it);
                loop->post(std::bind(&engine_t::rebalance_slaves, shared_from_this()));
                return true;
                break;
            default:
                BOOST_ASSERT(false);
            }
        }

        return false;
    });

    if (was_despawned) {
        observers.apply([&](const observers_type& observers) {
            for(auto& o : observers) {
                o->despawned(id);
            }
        });
    }
}

auto engine_t::on_handshake(const std::string& id, std::shared_ptr<session_t> session, upstream<io::worker::control_tag>&& stream)
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
        auto self = shared_from_this();
        loop->post([&, self] {
            rebalance_events();
        });

        observers.apply([&](const observers_type& observers) {
            for(auto& o : observers) {
                o->spawned({});
            }
        });
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

    observers.apply([&](const observers_type& observers) {
        for(auto& o : observers) {
            o->despawned(uuid);
        }
    });

    loop->post(std::bind(&engine_t::rebalance_slaves, shared_from_this()));
}

auto engine_t::on_spawn_rate_timeout(const std::error_code&) -> void {
    on_spawn_rate_timer.apply([&](std::unique_ptr<asio::deadline_timer>& timer) {
        timer.reset();
    });

    COCAINE_LOG_INFO(log, "rebalancing slaves after exponential backoff timeout");
    rebalance_slaves();
}

auto engine_t::select_slave(pool_type& pool) -> boost::optional<slave_t&> {
    auto concurrency = profile().concurrency;
    return select_slave(pool, [&](const slave_t& slave) -> bool {
        return slave.active() && slave.load() < concurrency;
    });
}

auto engine_t::select_slave(pool_type& pool, std::function<bool(const slave_t& slave)> filter) -> boost::optional<slave_t&> {
    auto range = pool | boost::adaptors::map_values | boost::adaptors::filtered(filter);

    auto slave = boost::min_element(range, +[](const slave_t& lhs, const slave_t& rhs) -> bool {
        return lhs.load() < rhs.load();
    });

    if (slave == boost::end(range)) {
        return boost::none;
    } else {
        return *slave;
    }
}

auto engine_t::pool_pressure(pool_type& pool) -> std::size_t {
    return boost::accumulate(pool |
        boost::adaptors::map_values |
        boost::adaptors::transformed(+[](const slave_t& slave) {
            return slave.load();
        }),
        0
    );
}

auto engine_t::rebalance_events() -> void {
    pool.apply([&](pool_type& pool) {
        queue.apply([&](queue_type& queue) {
            rebalance_events(pool, queue);
        });
    });
}

auto engine_t::rebalance_events(pool_type& pool, queue_type& queue) -> void {
    if (pool.empty()) {
        return;
    }

    COCAINE_LOG_DEBUG(log, "rebalancing events queue");
    while (!queue.empty()) {
        auto& load = queue.front();
        COCAINE_LOG_DEBUG(log, "rebalancing event");

        if (auto slave = select_slave(pool)) {
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
                break;
            }
        } else {
            COCAINE_LOG_DEBUG(log, "no free slaves found, rebalancing is over");
            break;
        }
    }
}

auto engine_t::rebalance_slaves() -> void {
    if (stopped) {
        return;
    }

    const auto load = queue->size();
    const auto profile = this->profile();

    const auto manual_target = static_cast<std::size_t>(this->pool_target.load());

    std::size_t target;
    if (manual_target > 0) {
        target = manual_target;
    } else {
        if (profile.queue_limit > 0) {
            target = load / profile.grow_threshold;
        } else {
            target = pool.apply([&](pool_type& pool) {
                auto pressure = pool_pressure(pool);
                auto vacant = pool.size() * profile.concurrency - pressure;

                std::size_t lack = 0;
                if (load < vacant) {
                    lack = 0;
                } else {
                    lack = std::ceil((load - vacant) / static_cast<double>(profile.concurrency));
                }

                return pool.size() +  lack;
            });
        }
    }

    // Bound current pool target between [1; limit].
    target = stdext::clamp(target, 1ul, profile.pool_limit);

    if (*on_spawn_rate_timer.synchronize()) {
        return;
    }

    pool.apply([&](pool_type& pool) {
        if (manual_target) {
            COCAINE_LOG_DEBUG(log, "attempting to rebalance slaves using direct policy", {
                {"load", load},
                {"slaves", pool.size()},
                {"target", target},
            });

            if (target <= pool.size()) {
                std::size_t active = boost::count_if(pool | boost::adaptors::map_values, +[](const slave_t& slave) -> bool {
                    return slave.active();
                });

                COCAINE_LOG_DEBUG(log, "sealing up to {} active slaves", active);

                while (active-- > target) {
                    // Find active slave with minimal load.
                    auto slave = select_slave(pool, +[](const slave_t& slave) -> bool {
                        return slave.active();
                    });

                    // All slaves are now inactive due to some external conditions.
                    if (!slave) {
                        break;
                    }

                    try {
                        COCAINE_LOG_DEBUG(log, "sealing slave", {{"uuid", slave->id()}});
                        slave->seal();
                    } catch (const std::exception& err) {
                        COCAINE_LOG_WARNING(log, "failed to seal slave: {}", err.what());
                    }
                }
            } else {
                while(pool.size() < target) {
                    spawn(pool);
                }
            }
        } else {
            COCAINE_LOG_DEBUG(log, "attempting to rebalance slaves using automatic policy", {
                {"load", load},
                {"slaves", pool.size()},
                {"target", target},
            });

            if (pool.size() >= profile.pool_limit) {
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
