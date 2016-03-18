#include <blackhole/logger.hpp>

#include <cocaine/logging.hpp>

#include "cocaine/detail/isolate/fetcher.hpp"
#include "cocaine/detail/service/node/slave/spawn_handle.hpp"

namespace cocaine {
namespace isolate {

namespace ph = std::placeholders;

fetcher_t::fetcher_t(asio::io_service& io_context,
                     std::shared_ptr<api::spawn_handle_base_t> _handle,
                     std::unique_ptr<logging::logger_t> _logger
):
    handle(std::move(_handle)),
    logger(std::move(_logger)),
    watcher(io_context)
{}

void
fetcher_t::assign(int fd) {
    watcher.apply([&](watcher_type& watcher) {
        watcher.assign(fd);
    });

    COCAINE_LOG_DEBUG(logger, "slave has started fetching standard output");
    watch();
}

void
fetcher_t::close() {
    watcher.apply([&](watcher_type& watcher) {
        if (watcher.is_open()) {
            COCAINE_LOG_DEBUG(logger, "slave has cancelled fetching standard output");

            try {
                watcher.close();
            } catch (const std::system_error& err) {
                COCAINE_LOG_WARNING(logger, "unable to close standard output watcher: {}", err.what());
            }
        }
    });
}

void
fetcher_t::watch() {
    COCAINE_LOG_DEBUG(logger, "slave is fetching more standard output");

    watcher.apply([&](watcher_type& watcher) {
        watcher.async_read_some(
            asio::buffer(buffer.data(), buffer.size()),
            std::bind(&fetcher_t::on_read, shared_from_this(), ph::_1, ph::_2)
        );
    });
}

void
fetcher_t::on_read(const std::error_code& ec, size_t len) {
    switch (ec.value()) {
    case 0:
        COCAINE_LOG_DEBUG(logger, "slave has received {} bytes of output", len);
        handle->on_data(std::string(buffer.data(), len));
        watch();
        break;
    case asio::error::operation_aborted:
        break;
    case asio::error::eof:
        COCAINE_LOG_DEBUG(logger, "slave has closed its output");
        break;
    default:
        COCAINE_LOG_WARNING(logger, "slave has failed to read output: {}", ec.message());
    }
}

}  // namespace isolate
}  // namespace cocaine
