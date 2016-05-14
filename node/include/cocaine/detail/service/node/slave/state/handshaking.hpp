#pragma once

#include <chrono>
#include <memory>

#include <asio/deadline_timer.hpp>

#include <cocaine/locked_ptr.hpp>

#include "state.hpp"

#include "cocaine/detail/service/node/forwards.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {
namespace state {

class handshaking_t : public state_t, public std::enable_shared_from_this<handshaking_t> {
    std::shared_ptr<machine_t> slave;

    synchronized<asio::deadline_timer> timer;
    std::unique_ptr<api::cancellation_t> handle;

    std::chrono::high_resolution_clock::time_point birthtime;

public:
    handshaking_t(std::shared_ptr<machine_t> slave, std::unique_ptr<api::cancellation_t> handle);

    auto name() const noexcept -> const char*;
    auto cancel() -> void;
    auto terminate(const std::error_code& ec) -> void;

    /// Activates the slave by transferring it to the active state using given session and control
    /// channel.
    ///
    /// \threadsafe
    virtual
    auto activate(std::shared_ptr<session_t> session, upstream<io::worker::control_tag> stream)
        -> std::shared_ptr<control_t>;

    auto activate(std::shared_ptr<session_t> session, std::shared_ptr<control_t> control) -> void;

    auto start(unsigned long timeout) -> void;

private:
    auto on_timeout(const std::error_code& ec) -> void;
};

}  // namespace state
}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
