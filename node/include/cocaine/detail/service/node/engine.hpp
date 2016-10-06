#pragma once

#include <deque>
#include <string>

#include <cocaine/rpc/dispatch.hpp>

#include "cocaine/idl/node.hpp"
#include "cocaine/idl/rpc.hpp"

#include "cocaine/service/node/app/event.hpp"
#include "cocaine/service/node/manifest.hpp"
#include "cocaine/service/node/profile.hpp"
#include "cocaine/service/node/slave/id.hpp"

#include "cocaine/detail/service/node/slave.hpp"
#include "cocaine/detail/service/node/slave/load.hpp"
#include "cocaine/detail/service/node/stats.hpp"

#include "node/pool_observer.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {

using cocaine::service::node::app::event_t;
using cocaine::service::node::slave::id_t;
using cocaine::service::node::pool_observer;

using slave::control_t;
using slave::load_t;

class engine_t : public std::enable_shared_from_this<engine_t> {
public:
    enum class despawn_policy_t { graceful, force };

    typedef std::deque<load_t> queue_type;
    typedef std::unordered_map<std::string, slave_t> pool_type;

    const std::unique_ptr<cocaine::logging::logger_t> log;

    context_t& context;

    std::atomic<bool> stopped;

    /// Time point, when the overseer was created.
    const std::chrono::system_clock::time_point birthstamp;

    /// The application manifest.
    const manifest_t manifest_;

    /// The application profile.
    synchronized<profile_t> profile_;

    /// IO loop for timers and standard output fetchers.
    std::shared_ptr<asio::io_service> loop;

    /// Slave pool.
    // TODO: Seems like we need multichannel queue system with size 1 and timeouts.
    synchronized<pool_type> pool;
    std::atomic<int> pool_target;
    synchronized<std::unique_ptr<asio::deadline_timer>> on_spawn_rate_timer;
    std::chrono::system_clock::time_point last_failed;
    std::chrono::seconds last_timeout;

    pool_observer& observer;

    /// Pending queue.
    synchronized<queue_type> queue;

    /// Statistics.
    stats_t stats;

public:
    engine_t(context_t& context,
             manifest_t manifest,
             profile_t profile,
             pool_observer& observer,
             std::shared_ptr<asio::io_service> loop);

    ~engine_t();

    auto active_workers() const -> std::uint32_t;

    /// Returns copy of the current manifest.
    ///
    /// Application's manifest is considered constant during all app's lifetime and can be
    /// changed only through restarting.
    auto manifest() const -> manifest_t;

    /// Returns copy of the current profile, which is used to spawn new slaves.
    ///
    /// \note the current profile may change in any moment. Moveover some slaves can be in some kind
    /// of transition state, i.e. migrating from one profile to another.
    auto profile() const -> profile_t;

    /// Returns application total uptime in seconds.
    auto uptime() const -> std::chrono::seconds;

    /// Returns the complete info about how the application works using json-like object.
    auto info(io::node::info::flags_t flags) const -> dynamic_t::object_t;

    // Modifiers.

    /// Enqueues the new event into the most appropriate slave.
    ///
    /// The event will be put into the queue if there are no slaves available at this moment or all
    /// of them are busy.
    ///
    /// \return the dispatch object, which is ready for processing the appropriate protocol
    /// messages.
    ///
    /// \param downstream represents the [Client <- Worker] stream.
    /// \param event an invocation event.
    /// \param id represents slave id to be enqueued (may be none, which means any slave).
    auto enqueue(upstream<io::stream_of<std::string>::tag> downstream, event_t event,
                 boost::optional<id_t> id) -> std::shared_ptr<client_rpc_dispatch_t>;

    /// Enqueues the new event into the most appropriate slave.
    ///
    /// The event will be put into the queue if there are no slaves available at this moment or all
    /// of them are busy.
    ///
    /// \param rx a receiver stream which methods will be called when the appropriate messages
    ///     received.
    /// \param event an invocation event.
    /// \param id represents slave id to be enqueued (may be none, which means any slave).
    /// \return a tx stream.
    auto enqueue(std::shared_ptr<api::stream_t> rx, event_t event, boost::optional<id_t> id) ->
        std::shared_ptr<api::stream_t>;

    /// Tries to keep alive at least `count` workers no matter what.
    ///
    /// Zero value is allowed and means not to spawn workers
    auto control_population(int count) -> void;

    /// Creates a new handshake dispatch, which will be consumed after a new incoming connection
    /// attached.
    ///
    /// This method is called when an unix-socket client (probably, a worker) has been accepted.
    /// The first message from it should be a handshake to be sure, that the remote peer is the
    /// worker we are waiting for.
    ///
    /// The handshake message should contain its peer id (likely uuid) by comparing that we either
    /// accept the session or drop it.
    ///
    /// \note after successful accepting the balancer will be notified about pool's changes.
    auto prototype() -> io::dispatch_ptr_t;

    /// Cancels all asynchronous pending operations, preparing for destruction.
    auto cancel() -> void;

private:
    /// Spawns a new slave using current manifest and profile.
    auto spawn(pool_type& pool) -> void;

    /// Spawns a new slave with the given id using current manifest and profile.
    auto spawn(const id_t& id, pool_type& pool) -> void;

    /// \warning must be called under the pool lock.
    auto assign(slave_t& slave, load_t& load) -> void;

    /// Seals the worker, preventing it from new requests.
    ///
    /// Then forces the slave to send terminate event. Starts the timer. On timeout or on response
    /// erases slave.
    auto despawn(const std::string& id, despawn_policy_t policy) -> void;

    auto on_handshake(const std::string& id, std::shared_ptr<session_t> session,
                      upstream<io::worker::control_tag>&& stream) -> std::shared_ptr<control_t>;

    auto on_slave_death(const std::error_code& ec, std::string uuid) -> void;

    auto rebalance_events() -> void;

    auto rebalance_slaves() -> void;

    auto on_spawn_rate_timeout(const std::error_code& ec) -> void;
};

}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
