#pragma once

#include <functional>
#include <string>
#include <system_error>

#include <boost/circular_buffer.hpp>
#include <boost/variant/variant.hpp>

#include <asio/io_service.hpp>
#include <asio/deadline_timer.hpp>
#include <asio/posix/stream_descriptor.hpp>

#include <cocaine/logging.hpp>
#include <cocaine/unique_id.hpp>

#include "cocaine/api/isolate.hpp"
#include "cocaine/idl/rpc.hpp"
#include "cocaine/idl/node.hpp"

#include "cocaine/detail/service/node/event.hpp"
#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"
#include "cocaine/detail/service/node/splitter.hpp"

#include "slave/error.hpp"

// TODO: Temporary.
#include "cocaine/detail/service/node/slot.hpp"

namespace cocaine {

class client_rpc_dispatch_t;

class active_t;
class stopped_t;
class channel_t;
class handshaking_t;
class spawning_t;
class state_t;
class terminating_t;

namespace service { namespace node { namespace slave { namespace state {
class sealing_t;
}}}}


class control_t;
class fetcher_t;

typedef std::shared_ptr<
    const dispatch<io::event_traits<io::worker::rpc::invoke>::dispatch_type>
> inject_dispatch_ptr_t;

typedef std::function<void()> close_callback;

namespace service { namespace node { namespace slave {

struct id_t {
    const std::string id;

    id_t(std::string id):
        id(std::move(id))
    {}

    static
    id_t
    generate() {
        return id_t(unique_id_t().string());
    }
};

}}}

namespace slave {

struct channel_t {
    /// Event to be processed.
    app::event_t event;

    std::shared_ptr<client_rpc_dispatch_t> dispatch;
    io::streaming_slot<io::app::enqueue>::upstream_type downstream;
};

struct stats_t {
    /// Current state name.
    std::string state;

    std::uint64_t tx;
    std::uint64_t rx;
    std::uint64_t load;
    std::uint64_t total;

    boost::optional<std::chrono::high_resolution_clock::time_point> age;

    stats_t();
};

} // namespace slave

struct slave_context {
    context_t&  context;
    manifest_t  manifest;
    profile_t   profile;
    std::string id;

    slave_context(context_t& context, manifest_t manifest, profile_t profile) :
        context(context),
        manifest(manifest),
        profile(profile),
        id(unique_id_t().string())
    {}
};

/// Actual slave implementation.
class state_machine_t:
    public std::enable_shared_from_this<state_machine_t>
{
    friend class active_t;
    friend class stopped_t;
    friend class handshaking_t;
    friend class spawning_t;
    friend class terminating_t;
    friend class service::node::slave::state::sealing_t;

    friend class control_t;
    friend class fetcher_t;

    friend class client_rpc_dispatch_t;

    friend class channel_t;

    class lock_t {};

public:
    typedef std::function<void(std::uint64_t)> channel_handler;
    typedef std::function<void(const std::error_code&)> cleanup_handler;

private:
    const std::unique_ptr<logging::log_t> log;

    const slave_context context;
    // TODO: In current implementation this can be invalid, when engine is stopped.
    asio::io_service& loop;

    /// The flag means that the overseer has been destroyed and we shouldn't call the callback.
    std::atomic<bool> closed;
    cleanup_handler cleanup;

    splitter_t splitter;
    synchronized<std::shared_ptr<fetcher_t>> fetcher;
    boost::circular_buffer<std::string> lines;

    std::atomic<bool> shutdowned;

    synchronized<std::shared_ptr<state_t>> state;

    std::atomic<std::uint64_t> counter;

    typedef std::unordered_map<std::uint64_t, std::shared_ptr<channel_t>> channels_map_t;

    struct {
        synchronized<channels_map_t> channels;
    } data;

public:
    /// Creates the state machine instance and immediately starts it.
    static
    std::shared_ptr<state_machine_t>
    create(slave_context context, asio::io_service& loop, cleanup_handler cleanup);

    state_machine_t(lock_t, slave_context context, asio::io_service& loop, cleanup_handler cleanup);

    ~state_machine_t();

    // Observers.

    /// Returns true is the slave is in active state.
    bool
    active() const noexcept;

    std::uint64_t
    load() const;

    slave::stats_t
    stats() const;

    const profile_t&
    profile() const;

    // Modifiers.

    std::shared_ptr<control_t>
    activate(std::shared_ptr<session_t> session, upstream<io::worker::control_tag> stream);

    std::uint64_t
    inject(slave::channel_t& channel, channel_handler handler);

    void
    seal();

    /// Terminates the slave by sending terminate message to the worker instance.
    ///
    /// The cleanup callback won't be called after this call.
    void
    terminate(std::error_code ec);

private:
    /// Spawns a slave.
    ///
    /// \pre state == nullptr.
    /// \post state != nullptr.
    void
    start();

    void
    output(const char* data, size_t size);

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

// TODO: Rename to `comrade`, because in Soviet Russia slave owns you!
class slave_t {
public:
    typedef state_machine_t::cleanup_handler cleanup_handler;

private:
    /// Termination reason.
    std::error_code ec;

    struct {
        std::string id;
        std::chrono::high_resolution_clock::time_point birthstamp;
    } data;

    /// The slave state machine implementation.
    std::shared_ptr<state_machine_t> machine;

public:
    slave_t(slave_context context, asio::io_service& loop, cleanup_handler fn);
    slave_t(const slave_t& other) = delete;
    slave_t(slave_t&&) = default;

    ~slave_t();

    slave_t& operator=(const slave_t& other) = delete;
    slave_t& operator=(slave_t&&) = default;

    // Observers.

    const std::string&
    id() const noexcept;

    long long
    uptime() const;

    std::uint64_t
    load() const;

    slave::stats_t
    stats() const;

    bool
    active() const noexcept;

    /// Returns the profile attached.
    profile_t
    profile() const;

    // Modifiers.

    std::shared_ptr<control_t>
    activate(std::shared_ptr<session_t> session, upstream<io::worker::control_tag> stream);

    std::uint64_t
    inject(slave::channel_t& channel, state_machine_t::channel_handler handler);

    void
    seal();

    /// Marks the slave for termination using the given error code.
    ///
    /// It will be terminated later in destructor.
    void
    terminate(std::error_code ec);
};

} // namespace cocaine
