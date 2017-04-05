#pragma once

#include <memory>
#include <system_error>

#include <cocaine/forwards.hpp>

#include "cocaine/idl/rpc.hpp"

#include "cocaine/detail/service/node/forwards.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {
namespace state {

class state_t {
public:
    virtual ~state_t() = 0;

    /// Returns state name as a null terminated string.
    virtual auto name() const noexcept -> const char* = 0;

    /// Checks whether current state is active.
    virtual auto active() const noexcept -> bool;

    /// Checks whether current state is sealing.
    virtual auto sealing() const noexcept -> bool;

    /// Checks whether current state is terminating.
    virtual auto terminating() const noexcept -> bool;

    /// Cancels all pending asynchronous operations.
    ///
    /// Default implementation does nothing.
    /// When required should be invoked on slave shutdown, indicating that the current state should
    /// cancel all its asynchronous operations to break cyclic references.
    virtual auto cancel() -> void;

    /// Activates the slave, making it possible to accept new requests.
    virtual auto activate(std::shared_ptr<session_t> session,
                          upstream<io::worker::control_tag> stream) -> std::shared_ptr<control_t>;

    /// Terminates the slave gracefully, allowing to process already accepted requests.
    virtual auto seal() -> void;

    /// Terminates the slave with the given error code.
    virtual auto terminate(const std::error_code& ec) -> void;

    virtual auto inject(std::shared_ptr<dispatch<io::stream_of<std::string>::tag>> dispatch)
        -> io::upstream_ptr_t;

private:
    auto __attribute__((noreturn)) throw_invalid_state() -> void;
};

}  // namespace state
}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
