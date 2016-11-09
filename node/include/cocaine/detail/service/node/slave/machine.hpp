#pragma once

#include <functional>
#include <string>
#include <system_error>

#include <boost/circular_buffer.hpp>

#include <asio/deadline_timer.hpp>
#include <asio/io_service.hpp>
#include <asio/posix/stream_descriptor.hpp>

#include <cocaine/logging.hpp>

#include "cocaine/api/auth.hpp"
#include "cocaine/idl/rpc.hpp"

#include "cocaine/service/node/app/event.hpp"
#include "cocaine/service/node/manifest.hpp"
#include "cocaine/service/node/profile.hpp"
#include "cocaine/service/node/slave/error.hpp"
#include "cocaine/service/node/slave/id.hpp"

#include "cocaine/detail/service/node/forwards.hpp"
#include "cocaine/detail/service/node/splitter.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {

using cocaine::service::node::slave::id_t;

using state::state_t;

/// Actual slave implementation.
class machine_t : public std::enable_shared_from_this<machine_t> {
    friend class state::active_t;
    friend class state::handshaking_t;
    friend class state::seal_t;
    friend class state::spawn_t;
    friend class state::state_t;
    friend class state::inactive_t;
    friend class state::terminate_t;

    friend class spawn_handle_t;

    friend class client_rpc_dispatch_t;

    friend class channel_t;
    friend class control_t;
    friend class fetcher_t;

public:
    typedef std::function<void(std::uint64_t)> channel_handler;
    typedef std::function<void(const std::error_code&)> cleanup_handler;

private:
    const std::unique_ptr<cocaine::logging::logger_t> log;

public:
    context_t& context;

    // TODO: Drop namespaces.
    const cocaine::service::node::slave::id_t id;
    const profile_t profile;
    const manifest_t manifest;
    std::shared_ptr<api::auth_t> auth;

private:
    // TODO: In current implementation this can be invalid, when engine is stopped.
    asio::io_service& loop;

    /// The flag means that the overseer has been destroyed and we shouldn't call the callback.
    std::atomic<bool> closed;
    cleanup_handler cleanup;

    splitter_t splitter;
    boost::circular_buffer<std::string> lines;

    std::atomic<bool> shutdowned;

    synchronized<std::shared_ptr<state_t>> state;

    std::atomic<std::uint64_t> counter;

    typedef std::unordered_map<std::uint64_t, std::shared_ptr<channel_t>> channels_map_t;
    typedef std::unordered_map<std::uint64_t, std::shared_ptr<asio::deadline_timer>> timers_map_t;

    struct {
        synchronized<channels_map_t> channels;
        synchronized<timers_map_t> timers;
    } data;

public:
    machine_t(context_t& context,
              id_t id,
              manifest_t manifest,
              profile_t profile,
              std::shared_ptr<api::auth_t> auth,
              asio::io_service& loop,
              cleanup_handler cleanup);

    ~machine_t();

    // Observers.

    /// Returns true is the slave is in active state.
    bool
    active() const noexcept;

    std::uint64_t
    load() const;

    detail::service::node::slave::stats_t
    stats() const;

    // Modifiers.

    std::shared_ptr<control_t>
    activate(std::shared_ptr<session_t> session, upstream<io::worker::control_tag> stream);

    // TODO: Drop typedef, rename to "callback".
    std::uint64_t
    inject(load_t& load, channel_handler handler);

    void
    seal();

    /// Terminates the slave by sending terminate message to the worker instance.
    ///
    /// The cleanup callback won't be called after this call.
    void
    terminate(std::error_code ec);

    /// Spawns a slave.
    ///
    /// \pre state == nullptr.
    /// \post state != nullptr.
    void
    start();

private:
    void
    output(const char* data, size_t size);

    void
    output(const std::string& data);

    void
    migrate(std::shared_ptr<state_t> desired);

    /// Internal termination.
    ///
    /// Can be called multiple times, but only the first one takes an effect.
    void
    shutdown(std::error_code ec);

    void
    revoke(std::uint64_t id, channel_handler handler);

    void
    dump();
};

}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
