#pragma once

#include <string>

#include <cocaine/rpc/dispatch.hpp>

#include "cocaine/idl/node.hpp"
#include "cocaine/idl/rpc.hpp"

#include "cocaine/service/node/app/event.hpp"
#include "cocaine/service/node/manifest.hpp"
#include "cocaine/service/node/profile.hpp"
#include "cocaine/service/node/slave/id.hpp"

#include "forwards.hpp"

namespace cocaine {
namespace service {
namespace node {

class pool_observer;

class overseer_t {
    std::shared_ptr<engine_t> engine;

public:
    overseer_t(context_t& context,
               manifest_t manifest,
               profile_t profile,
               std::shared_ptr<pool_observer> observer,
               std::shared_ptr<asio::io_service> loop);

    /// TODO: Docs.
    ~overseer_t();

    /// Returns application total uptime in seconds.
    auto uptime() const -> std::chrono::seconds;

    /// Returns number of currently active workers.
    auto active_workers() const -> std::uint32_t;

    /// Returns the copy of current profile, which is used to spawn new slaves.
    ///
    /// \note current profile may be changed at any moment.
    ///     Moveover some slaves can be in some kind of transition state, i.e. migrating from one
    ///     profile to another.
    auto profile() const -> profile_t;

    /// Returns the copy of current manifest.
    ///
    /// Application's manifest is considered constant during all app's lifetime and can be
    /// changed only through restarting.
    auto manifest() const -> manifest_t;

    /// Returns the complete info about how the application works using json-like object.
    // TODO: Flags are bad.
    auto info(io::node::info::flags_t flags) const -> dynamic_t::object_t;

    // Modifiers.

    /// Enqueues the new event into the most appropriate slave.
    ///
    /// The event will be put into the queue if there are no slaves available at this moment or all
    /// of them are busy.
    ///
    /// \param downstream represents the [Client <- Worker] stream.
    /// \param event an invocation event.
    ///
    /// \return the dispatch object, which is ready for processing the appropriate protocol
    ///     messages.
    auto enqueue(app::event_t event, upstream<io::stream_of<std::string>::tag> downstream)
        -> std::shared_ptr<client_rpc_dispatch_t>;

    /// Enqueues the new event into the most appropriate slave.
    ///
    /// The event will be put into the queue if there are no slaves available at this moment or all
    /// of them are busy.
    ///
    /// \param rx a receiver stream which methods will be called when the appropriate messages
    ///     received.
    /// \param event an invocation event.
    ///
    /// \return a tx stream.
    auto enqueue(app::event_t event, std::shared_ptr<api::stream_t> rx)
        -> std::shared_ptr<api::stream_t>;

    /// Tries to keep alive at least `count` workers no matter what.
    ///
    /// Zero value is allowed and means not to spawn workers at all.
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
    auto prototype() -> io::dispatch_ptr_t;
};

}  // namespace node
}  // namespace service
}  // namespace cocaine
