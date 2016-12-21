#pragma once

#include "cocaine/vicodyn/forwards.hpp"
#include "cocaine/vicodyn/proxy/appendable.hpp"

#include <cocaine/forwards.hpp>
#include <cocaine/hpack/header.hpp>
#include <cocaine/locked_ptr.hpp>
#include <cocaine/rpc/asio/encoder.hpp>

#include <msgpack/unpack.hpp>

namespace cocaine {
namespace vicodyn {
namespace queue {

class send_t: public proxy::appendable_t {
public:
    auto append(const msgpack::object& message, uint64_t event_id, hpack::header_storage_t headers) -> void override;

    auto attach(io::upstream_ptr_t upstream) -> void;

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

    // Operation log.
    std::vector<operation_t> operations;

    // The upstream might be attached during message invocation, so it has to be synchronized for
    // thread safety - the atomicity guarantee of the shared_ptr<T> is not enough.
    io::upstream_ptr_t upstream;
};

} // namespace queue
} // namespace vicodyn
} // namespace cocaine
