#pragma once

#include "cocaine/vicodyn/forwards.hpp"

#include <cocaine/forwards.hpp>
#include <cocaine/hpack/header.hpp>
#include <cocaine/locked_ptr.hpp>
#include <cocaine/rpc/asio/encoder.hpp>

#include <msgpack/unpack.hpp>

namespace cocaine {
namespace vicodyn {

class stream_t {
public:
    enum class direction_t {
        forward,
        backward
    };
    stream_t(direction_t);
    stream_t(const stream_t&) = delete;
    stream_t& operator=(const stream_t&) = delete;
    stream_t(stream_t&&) = default;
    stream_t& operator=(stream_t&&) = default;

    auto attach(io::upstream_ptr_t wrapped_stream) -> void;
    auto attach(std::shared_ptr<session_t> session) -> void;

    auto append(const msgpack::object& message, uint64_t event_id, hpack::header_storage_t headers) -> void;

    auto discard(std::error_code) -> void;

private:
    struct operation_t {
        // msgpack object
        msgpack::object data;

        // msgpack zone where all pointers are located
        std::unique_ptr<msgpack::zone> zone;

        // pack-unpack buffer which stores raw data. msgpack objects point to this buffer
        io::aux::encoded_message_t encoded_message;

        // id of the message event
        uint64_t event_id;

        // message headers
        hpack::header_storage_t headers;
    };

    auto try_discard() -> void;

    synchronized<void> attach_mutex;
    direction_t direction;
    boost::optional<std::error_code> discard_code;

    // Operation log.
    std::vector<operation_t> operations;
    io::upstream_ptr_t wrapped_stream;
    std::shared_ptr<session_t> session;
};

} // namespace vicodyn
} // namespace cocaine
