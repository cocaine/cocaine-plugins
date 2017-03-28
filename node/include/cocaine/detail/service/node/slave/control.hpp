#pragma once

#include <functional>

#include <cocaine/rpc/dispatch.hpp>

#include "cocaine/idl/rpc.hpp"

#include "cocaine/detail/service/node/forwards.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {

/// Control channel for single slave.
///
/// \note worker should shut itself down after sending terminate message back (even if it initiates)
/// to the runtime.
class control_t : public dispatch<io::worker::control_tag>,
                  public std::enable_shared_from_this<control_t> {
    /// Attached slave.
    std::shared_ptr<machine_t> slave;

    /// Upstream to send messages to the worker.
    upstream<io::worker::control_tag> stream;

    /// Heartbeat timer.
    // TODO: Need synchronization.
    asio::deadline_timer timer;

    std::atomic<bool> closed;

public:
    control_t(std::shared_ptr<machine_t> slave, upstream<io::worker::control_tag> stream);

    virtual ~control_t();

    /// Starts health checking explicitly.
    auto start() -> void;

    /// Sends terminate event to the slave.
    auto terminate(const std::error_code& ec) -> void;

    /// Cancels all asynchronous operations on channel (e.g. timers).
    ///
    /// \note this method is required to be explicitly called on slave shutdown, because it breaks
    /// all cycle references inside the control channel.
    auto cancel() -> void;

    /// Called on any I/O error.
    virtual auto discard(const std::error_code& ec) -> void;

private:
    auto on_heartbeat() -> void;
    auto on_terminate(int ec, const std::string& reason) -> void;
    auto on_timeout(const std::error_code& ec) -> void;
};

}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
