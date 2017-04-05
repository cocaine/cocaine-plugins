#pragma once

#include "cocaine/vicodyn/forwards.hpp"
#include "cocaine/vicodyn/session.hpp"
#include "cocaine/vicodyn/stream.hpp"

#include <cocaine/forwards.hpp>
#include <cocaine/hpack/header.hpp>
#include <cocaine/rpc/asio/encoder.hpp>
#include <cocaine/rpc/graph.hpp>

#include <msgpack/unpack.hpp>

namespace cocaine {
namespace vicodyn {
namespace queue {

class invocation_t {
public:

    auto append(const msgpack::object& message, uint64_t event_id, hpack::header_storage_t headers,
                const io::graph_node_t& protocol, stream_ptr_t backward_stream) -> stream_ptr_t;

    auto absorb(invocation_t&& queue) -> void;

    auto attach(std::shared_ptr<session_t> session) -> void;

    auto connected() -> bool;

private:
    struct operation_t {
        // msgpack object
        msgpack::object data;

        // msgpack zone where all pointers are located
        std::unique_ptr<msgpack::zone> zone;

        // pack-unpack buffer which stores raw data. msgpack objects point to this buffer
        io::aux::encoded_message_t encoded_message;

        // message type
        uint64_t event_id;

        // message headers
        hpack::header_storage_t headers;

        // incoming protocol to fork session to
        const io::graph_node_t* incoming_protocol;

        //send queue to respond to
        stream_ptr_t backward_stream;

        // send queue to queue all further send invocations
        stream_ptr_t forward_stream;
    };

    auto execute(std::shared_ptr<session_t> session, const operation_t& op) -> void;

    std::deque<operation_t> m_operations;
    synchronized<std::shared_ptr<session_t>> m_session;

};

} // namespace queue
} // namespace vicodyn
} // namespace cocaine
