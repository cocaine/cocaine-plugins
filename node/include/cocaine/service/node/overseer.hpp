#pragma once

#include <deque>
#include <string>

#include <cocaine/rpc/dispatch.hpp>

#include "cocaine/idl/node.hpp"
#include "cocaine/idl/rpc.hpp"

#include "cocaine/detail/service/node/app/stats.hpp"
#include "cocaine/detail/service/node/event.hpp"
#include "cocaine/detail/service/node/slave.hpp"
#include "cocaine/detail/service/node/slot.hpp"

namespace cocaine {
    class client_rpc_dispatch_t;
    class control_t;
    class slave_t;
    class unix_actor_t;
}  // namespace cocaine

namespace cocaine {
namespace api {

class stream_t;

}  // namespace api
}  // namespace cocaine

namespace cocaine {

class overseer_t:
    public std::enable_shared_from_this<overseer_t>
{
public:
    enum class despawn_policy_t {
        graceful,
        force
    };

    typedef std::unordered_map<
        std::string,
        slave_t
    > pool_type;

    struct channel_wrapper_t {
        slave::channel_t channel;
        trace_t trace;

        slave::channel_t&
        operator*() {
            return channel;
        }

        const slave::channel_t&
        operator*() const {
            return channel;
        }

        slave::channel_t*
        operator->() {
            return &channel;
        }

        const slave::channel_t*
        operator->() const {
            return &channel;
        }
    };

    typedef std::deque<
        channel_wrapper_t
    > queue_type;

private:
    const std::unique_ptr<logging::logger_t> log;

    context_t& context;

    /// Time point, when the overseer was created.
    const std::chrono::system_clock::time_point birthstamp;

    /// The application manifest.
    const manifest_t manifest_;

    /// The application profile.
    synchronized<profile_t> profile_;

    /// IO loop for timers and standard output fetchers.
    std::shared_ptr<asio::io_service> loop;

    /// Slave pool.
    synchronized<pool_type> pool;
    std::atomic<int> pool_target;

    /// Pending queue.
    synchronized<queue_type> queue;

    /// Statistics.
    stats_t stats;

public:
    overseer_t(context_t& context, manifest_t manifest, profile_t profile,
        std::shared_ptr<asio::io_service> loop);

    ~overseer_t();

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
    ///
    /// \todo consult with E. guys about deadline policy.
    std::shared_ptr<client_rpc_dispatch_t>
    enqueue(io::streaming_slot<io::app::enqueue>::upstream_type downstream,
            app::event_t event,
            boost::optional<service::node::slave::id_t> id);

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
    auto enqueue(std::shared_ptr<api::stream_t> rx,
        app::event_t event,
        boost::optional<service::node::slave::id_t> id) -> std::shared_ptr<api::stream_t>;

    /// Tries to keep alive at least `count` workers no matter what.
    ///
    /// Zero value is allowed and means not to spawn workers
    void
    keep_alive(int count);

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
    io::dispatch_ptr_t
    prototype();

    /// Cancels all asynchronous pending operations, preparing for destruction.
    void
    cancel();

private:
    /// Spawns a new slave using current manifest and profile.
    void
    spawn(pool_type& pool);

    /// \warning must be called under the pool lock.
    void
    assign(slave_t& slave, slave::channel_t& payload);

    /// Seals the worker, preventing it from new requests.
    ///
    /// Then forces the slave to send terminate event. Starts the timer. On timeout or on response
    /// erases slave.
    void
    despawn(const std::string& id, despawn_policy_t policy);

    std::shared_ptr<control_t>
    on_handshake(const std::string& id,
                 std::shared_ptr<session_t> session,
                 upstream<io::worker::control_tag>&& stream);

    void
    on_slave_death(const std::error_code& ec, std::string uuid);

    void
    rebalance_events();

    void
    rebalance_slaves();
};

}
