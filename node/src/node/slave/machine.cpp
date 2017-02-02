#include "cocaine/detail/service/node/slave.hpp"

#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/min_element.hpp>

#include <blackhole/logger.hpp>

#include <metrics/gauge.hpp>
#include <metrics/registry.hpp>

#include <cocaine/context.hpp>
#include <cocaine/rpc/actor.hpp>
#include <cocaine/service/node/profile.hpp>

#include "cocaine/api/auth.hpp"
#include "cocaine/api/isolate.hpp"
#include "cocaine/api/stream.hpp"

#include "cocaine/service/node/manifest.hpp"
#include "cocaine/service/node/profile.hpp"
#include "cocaine/service/node/slave/id.hpp"

#include "cocaine/detail/service/node/dispatch/client.hpp"
#include "cocaine/detail/service/node/dispatch/worker.hpp"
#include "cocaine/detail/service/node/slave/channel.hpp"
#include "cocaine/detail/service/node/slave/control.hpp"
#include "cocaine/detail/isolate/fetcher.hpp"
#include "cocaine/detail/service/node/slave/load.hpp"
#include "cocaine/detail/service/node/slave/machine.hpp"
#include "cocaine/detail/service/node/slave/state/inactive.hpp"
#include "cocaine/detail/service/node/slave/state/preparation.hpp"
#include "cocaine/detail/service/node/slave/state/spawn.hpp"
#include "cocaine/detail/service/node/slave/stats.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {

namespace ph = std::placeholders;

using blackhole::attribute_list;

using detail::service::node::slave::state::spawn_t;
using detail::service::node::slave::state::preparation_t;
using detail::service::node::slave::state::inactive_t;

using cocaine::detail::service::node::slave::stats_t;
using cocaine::service::node::slave::id_t;

machine_t::metrics_t::metrics_t(context_t& context, std::shared_ptr<machine_t> parent) :
    prefix(cocaine::format("{}.pool.slaves.{}", parent->manifest.name, parent->id.id())),
    state(context.metrics_hub().register_gauge<std::string>(format("{}.state", prefix), {}, [=] {
        return parent->state->get()->name();
    })),
    uptime(context.metrics_hub().register_gauge<std::uint64_t>(format("{}.uptime", prefix), {}, [=] {
        return parent->uptime().count();
    }))
{}

machine_t::machine_t(context_t& context,
                     id_t id,
                     manifest_t manifest,
                     profile_t profile,
                     std::shared_ptr<api::auth_t> auth,
                     asio::io_service& loop,
                     cleanup_handler cleanup):
    log(context.log(format("{}/slave", manifest.name), {{ "uuid", id.id() }})),
    context(context),
    id(id),
    profile(profile),
    manifest(manifest),
    auth(std::move(auth)),
    loop(loop),
    closed(false),
    cleanup(std::move(cleanup)),
    lines(profile.crashlog_limit),
    shutdowned(false),
    counter(1),
    birthstamp(clock_type::now()),
    metrics(nullptr)
{
    COCAINE_LOG_DEBUG(log, "slave state machine has been initialized");
}

machine_t::~machine_t() {
    COCAINE_LOG_DEBUG(log, "slave state machine has been destroyed");
}

void
machine_t::start() {
    BOOST_ASSERT(*state.synchronize() == nullptr);

    COCAINE_LOG_DEBUG(log, "slave state machine is starting");

    auto preparation = std::make_shared<preparation_t>(shared_from_this());
    migrate(preparation);

    metrics.reset(new metrics_t(context, shared_from_this()));

    // NOTE: Slave spawning involves starting timers and posting completion handlers callbacks with
    // the event loop.
    loop.post(trace_t::bind([=]() {
        // This call can perform state machine shutdowning on any error occurred.
        preparation->start(std::chrono::milliseconds(profile.timeout.spawn));
    }));
}

auto
machine_t::uptime() const -> std::chrono::seconds {
    return std::chrono::duration_cast<
        std::chrono::seconds
    >(clock_type::now() - birthstamp);
}

bool
machine_t::active() const noexcept {
    auto state = *this->state.synchronize();
    BOOST_ASSERT(state);

    return state->active();
}

std::uint64_t
machine_t::load() const {
    return data.channels->size();
}

auto machine_t::stats() const -> stats_t {
    stats_t result;

    result.state = (*state.synchronize())->name();

    data.channels.apply([&](const channels_map_t& channels) {
        for (const auto& channel : channels) {
            if (channel.second->send_closed()) {
                ++result.tx;
            }

            if (channel.second->recv_closed()) {
                ++result.rx;
            }
        }

        result.load = channels.size();
        result.total = counter - 1;

        typedef std::shared_ptr<channel_t> value_type;

        const auto range = channels | boost::adaptors::map_values;
        const auto channel = boost::min_element(range, +[](const value_type& cur, const value_type& first) -> bool {
            return cur->birthstamp() < first->birthstamp();
        });

        if (channel != boost::end(range)) {
            result.age.reset((*channel)->birthstamp());
        }
    });

    return result;
}

std::shared_ptr<control_t>
machine_t::activate(std::shared_ptr<session_t> session, upstream<io::worker::control_tag> stream) {
    auto state = *this->state.synchronize();
    BOOST_ASSERT(state);

    return state->activate(std::move(session), std::move(stream));
}

auto machine_t::inject(load_t& load, channel_handler handler) -> std::uint64_t {
    const auto id = ++counter;

    auto channel = std::make_shared<channel_t>(
        id,
        load.event.birthstamp,
        std::bind(&machine_t::revoke, shared_from_this(), id, handler)
    );

    // W2C dispatch.
    auto dispatch = std::make_shared<const worker_rpc_dispatch_t>(
        load.downstream, [=](const std::error_code& ec) {
            if (ec) {
                channel->close_both();
            } else {
                channel->close_recv();
            }
        }
    );

    auto state = *this->state.synchronize();
    auto upstream = state->inject(dispatch);
    upstream->send<io::worker::rpc::invoke>(load.event.headers, load.event.name);

    channel->into_worker = load.dispatch;
    channel->from_worker = dispatch;

    const auto current = data.channels.apply([&](channels_map_t& channels) -> std::uint64_t {
        channels[id] = channel;
        return channels.size();
    });

    std::chrono::milliseconds request_timeout(profile.request_timeout());
    if (auto timeout_from_header = hpack::header::convert_first<std::uint64_t>(load.event.headers, "request_timeout")) {
        request_timeout = std::chrono::milliseconds(*timeout_from_header);
    }
    // May be negative. Probably, it's OK for boost::asio.
    const auto duration = std::chrono::duration_cast<
        std::chrono::milliseconds
    >(load.event.birthstamp + request_timeout - std::chrono::high_resolution_clock::now()).count();

    auto into_worker_dispatch = load.dispatch;
    auto from_worker_dispatch = dispatch;

    if (duration <= 0) {
        COCAINE_LOG_ERROR(log, "channel {} has timed out immediately, closing", id);
        into_worker_dispatch->discard(error::timeout_error);
        from_worker_dispatch->discard(error::timeout_error);
    } else {
        auto this_ = shared_from_this();

        auto timer = std::make_shared<asio::deadline_timer>(loop);
        data.timers.apply([&](timers_map_t& timers) {
            timers[id] = timer;
        });
        timer->expires_from_now(boost::posix_time::milliseconds(duration));
        timer->async_wait([=](const std::error_code& ec) mutable {
            if (ec == asio::error::operation_aborted) {
                return;
            }

            COCAINE_LOG_ERROR(this_->log, "channel {} has timed out, closing", id);
            into_worker_dispatch->discard(error::timeout_error);
            from_worker_dispatch->discard(error::timeout_error);
        });
    }

    COCAINE_LOG_DEBUG(log, "slave has started processing {} channel", id);

    COCAINE_LOG_DEBUG(log, "slave has increased its load to {}", current, attribute_list({{"channel", id}}));

    // C2W dispatch.
    load.dispatch->attach(upstream, [=](const std::error_code& ec) {
        if (ec) {
            channel->close_both();
        } else {
            channel->close_send();
        }
    });

    channel->watch();

    return id;
}

void
machine_t::seal() {
    auto state = *this->state.synchronize();

    state->seal();
}

void
machine_t::terminate(std::error_code ec) {
    BOOST_ASSERT(ec);
    metrics.reset();

    if (closed.exchange(true)) {
        return;
    }

    COCAINE_LOG_DEBUG(log, "slave state machine is terminating: {}", ec.message());

    auto state = *this->state.synchronize();
    state->terminate(ec);
}

void
machine_t::output(const char* data, size_t size) {
    output(std::string(data, size));
}

void
machine_t::output(const std::string& data) {
    splitter.consume(data);
    while (auto line = splitter.next()) {
        lines.push_back(*line);

        if (profile.log_output) {
            COCAINE_LOG_DEBUG(log, "slave's output: `{}`", *line);
        }
    }
}

void
machine_t::migrate(std::shared_ptr<state_t> target) {
    BOOST_ASSERT(target);

    state.apply([&](std::shared_ptr<state_t>& state) {
        COCAINE_LOG_DEBUG(log, "slave has changed its state from '{}' to '{}'",
            state ? state->name() : "null", target->name());
        state.swap(target);
    });
}

void
machine_t::shutdown(std::error_code ec) {
    if (shutdowned.exchange(true)) {
        return;
    }

    auto state = *this->state.synchronize();
    COCAINE_LOG_DEBUG(log, "slave is shutting down from state {}: {}", state->name(), ec.message());

    state->cancel();
    if(state->terminating()) {
        // We don't consider any reason for termination in "terminating" state as an error
        ec.clear();
    }
    migrate(std::make_shared<inactive_t>(ec));

    if (ec && ec != error::overseer_shutdowning) {
        dump();
    }

    data.timers.apply([&](timers_map_t& timers){
        timers.clear();
    });

    data.channels.apply([&](channels_map_t& channels) {
        const auto size = channels.size();
        if (size > 0) {
            COCAINE_LOG_WARNING(log, "slave is dropping {} sessions", size);
        }

        for (auto& channel : channels) {
            loop.post([=]() {
                channel.second->close_both();
            });
        }

        channels.clear();
    });


    // Check if the slave has been terminated externally. If so, do not call the cleanup callback.
    if (closed) {
        return;
    }

    // NOTE: To prevent deadlock between session.channels and overseer.pool. Consider some
    // other solution.
    const auto cleanup_handler = cleanup;
    loop.post([=]() {
        try {
            cleanup_handler(ec);
        } catch (const std::exception& err) {
            // Just eat an exception, we don't care why the cleanup handler failed to do its job.
            COCAINE_LOG_WARNING(log, "unable to cleanup after slave's death: {}", err.what());
        }
    });
}

void
machine_t::revoke(std::uint64_t id, channel_handler handler) {
    const auto load = data.channels.apply([&](channels_map_t& channels) -> std::uint64_t {
        channels.erase(id);
        return channels.size();
    });
    data.timers.apply([&](timers_map_t& timers) {
        timers.erase(id);
    });

    COCAINE_LOG_DEBUG(log, "slave has decreased its load to {}", load, attribute_list({{"channel", id}}));
    COCAINE_LOG_DEBUG(log, "slave has closed its {} channel", id);

    // Terminate the state machine if the current state is sealing and there are no more channels
    // left.
    {
        auto state = *this->state.synchronize();
        if (state->sealing() && data.channels->empty()) {
            COCAINE_LOG_DEBUG(log, "sealing completed");
            state->terminate(error::slave_is_sealing);
        }
    }

    handler(id);
}

void
machine_t::dump() {
    if (lines.empty() && splitter.empty()) {
        COCAINE_LOG_WARNING(log, "рабъ умеръ въ тишинѣ");
        return;
    }

    std::vector<std::string> dump;
    std::copy(lines.begin(), lines.end(), std::back_inserter(dump));

    if (!splitter.empty()) {
        dump.emplace_back(splitter.data());
    }

    const auto now = std::chrono::system_clock::now().time_since_epoch();

    const auto us = std::chrono::duration_cast<
        std::chrono::microseconds
    >(now).count();

    const auto key = format("{}:{}", us, id.id());

    std::vector<std::string> indexes{manifest.name};

    std::time_t time = std::time(nullptr);
    char buf[64];
    if (auto len = std::strftime(buf, sizeof(buf), "cocaine-%Y-%m-%d", std::gmtime(&time))) {
        indexes.emplace_back(buf, len);
    }

    COCAINE_LOG_INFO(log, "slave is dumping output to 'crashlogs/{}' using [{}] indexes",
                     key, boost::join(indexes, ", "));

    // TODO: this one is really only used to log any error during crashlog write. Maybe we can do it in a better way
    auto self = shared_from_this();
    api::storage(context, "core")->put("crashlogs", key, dump, indexes, [=](std::future<void> f){
        try {
            f.get();
        } catch (const std::exception& e) {
            COCAINE_LOG_WARNING(self->log, "slave is unable to save the crashlog: {}", e.what());
        }
    });
}

}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
