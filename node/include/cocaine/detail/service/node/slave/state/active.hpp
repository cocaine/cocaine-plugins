#pragma once

#include <memory>

#include "state.hpp"

#include "cocaine/detail/service/node/forwards.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {
namespace state {

/// Represents an active slave state, i.e. slave, that can accept and process new channels.
class active_t : public state_t, public std::enable_shared_from_this<active_t> {
    std::shared_ptr<machine_t> slave;
    std::unique_ptr<api::cancellation_t> handle;
    std::shared_ptr<session_t> session;
    std::shared_ptr<control_t> control;

public:
    active_t(std::shared_ptr<machine_t> slave, std::unique_ptr<api::cancellation_t> handle,
             std::shared_ptr<session_t> session, std::shared_ptr<control_t> control);

    ~active_t();

    auto name() const noexcept -> const char*;
    auto active() const noexcept -> bool;
    auto inject(std::shared_ptr<dispatch<io::stream_of<std::string>::tag>> dispatch)
        -> io::upstream_ptr_t;
    auto seal() -> void;

    /// Migrates the current state to the terminating one.
    ///
    /// This is achieved by sending the terminate event to the slave and waiting for its response
    /// for a specified amount of time (timeout.terminate).
    ///
    /// The slave also becomes inactive (i.e. unable to handle new channels).
    ///
    /// If the slave is unable to ack the termination event it will be considered as broken and
    /// should be removed from the pool.
    ///
    /// \warning this call invalidates the current object.
    auto terminate(const std::error_code& ec) -> void;
};

}  // namespace state
}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
