#pragma once

#include <array>
#include <memory>

#include <asio/posix/stream_descriptor.hpp>

#include <cocaine/locked_ptr.hpp>

#include "cocaine/detail/service/node/forwards.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {

/// Represents slave's standard output fetcher.
///
/// \warning all methods must be called from the event loop thread, otherwise the behavior is
/// undefined.
class fetcher_t : public std::enable_shared_from_this<fetcher_t> {
    std::shared_ptr<machine_t> slave;

    std::array<char, 4096> buffer;

    typedef asio::posix::stream_descriptor watcher_type;
    synchronized<watcher_type> watcher;

public:
    explicit fetcher_t(std::shared_ptr<machine_t> slave);

    /// Assigns an existing native descriptor to the output watcher and starts watching over it.
    ///
    /// \throws std::system_error on any system error while assigning an fd.
    auto assign(int fd) -> void;

    /// Cancels all asynchronous operations associated with the descriptor by closing it.
    auto close() -> void;

private:
    auto watch() -> void;

    auto on_read(const std::error_code& ec, size_t len) -> void;
};

}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
