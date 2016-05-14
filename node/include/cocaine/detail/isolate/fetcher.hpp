#pragma once

#include <array>
#include <memory>

#include <asio/posix/stream_descriptor.hpp>

#include <cocaine/forwards.hpp>
#include <cocaine/locked_ptr.hpp>

#include "cocaine/detail/service/node/forwards.hpp"

namespace cocaine {
namespace isolate {

/// Represents slave's standard output fetcher.
///
/// \warning all methods must be called from the event loop thread, otherwise the behavior is
/// undefined.
class fetcher_t : public std::enable_shared_from_this<fetcher_t> {
    std::shared_ptr<api::spawn_handle_base_t> handle;
    std::unique_ptr<logging::logger_t> logger;

    std::array<char, 4096> buffer;

    typedef asio::posix::stream_descriptor watcher_type;
    synchronized<watcher_type> watcher;

public:
    fetcher_t(asio::io_service& io_context,
              std::shared_ptr<api::spawn_handle_base_t> handle,
              std::unique_ptr<logging::logger_t> logger);

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

}  // namespace isolate
}  // namespace cocaine
