#include "cocaine/detail/service/node/dispatch/worker.hpp"

#include "cocaine/api/stream.hpp"

using namespace cocaine;

worker_rpc_dispatch_t::worker_rpc_dispatch_t(std::shared_ptr<stream_t> stream_, callback_type callback):
    dispatch<incoming_tag>("W2C"),
    stream(stream_), // NOTE: Intentionally copy here to provide exception-safety guarantee.
    state(state_t::open),
    callback(callback)
{
    on<protocol::chunk>([&](const std::string& chunk) {
        std::lock_guard<std::mutex> lock(mutex);

        if (state == state_t::closed) {
            return;
        }

        try {
            stream->write({}, chunk);
        } catch (const std::system_error&) {
            finalize(lock, asio::error::connection_aborted);
        }
    });

    on<protocol::error>([&](const std::error_code& ec, const std::string& reason) {
        std::lock_guard<std::mutex> lock(mutex);

        if (state == state_t::closed) {
            return;
        }

        try {
            stream->error({}, ec, reason);
            finalize(lock);
        } catch (const std::system_error&) {
            finalize(lock, asio::error::connection_aborted);
        }
    });

    on<protocol::choke>([&]() {
        std::lock_guard<std::mutex> lock(mutex);

        if (state == state_t::closed) {
            return;
        }

        try {
            stream->close({});
            finalize(lock);
        } catch (const std::system_error&) {
            finalize(lock, asio::error::connection_aborted);
        }
    });
}

void
worker_rpc_dispatch_t::discard(const std::error_code& ec) const {
    // TODO: Consider something less weird.
    const_cast<worker_rpc_dispatch_t*>(this)->discard(ec);
}

void
worker_rpc_dispatch_t::discard(const std::error_code& ec) {
    if (ec) {
        std::lock_guard<std::mutex> lock(mutex);

        if (state == state_t::closed) {
            return;
        }

        try {
            stream->error({}, ec, "slave has been discarded");
        } catch (const std::exception&) {
            // Eat.
        }

        finalize(lock);
    }
}

void
worker_rpc_dispatch_t::finalize(std::lock_guard<std::mutex>&, const std::error_code& ec) {
    // Ensure that we call this method only once no matter what.
    if (state == state_t::closed) {
        // TODO: Log the error.
        return;
    }

    state = state_t::closed;
    // TODO: We have to send the error to the worker on any error occurred while writing to the
    // client stream, otherwise it may push its messages to the Hell forever.
    callback(ec);
}
