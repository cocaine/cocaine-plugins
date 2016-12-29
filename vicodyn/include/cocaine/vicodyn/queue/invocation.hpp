#pragma once

#include "cocaine/vicodyn/forwards.hpp"
#include "cocaine/vicodyn/queue/send.hpp"

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
    std::shared_ptr<queue::send_t>
    append(const msgpack::object& message,
           uint64_t event_id,
           hpack::header_storage_t headers,
           const io::graph_node_t& protocol,
           io::upstream_ptr_t downstream);

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

        //invocation upstream to respond to
        io::upstream_ptr_t upstream;

        // incoming protocol to fork session to
        const io::graph_node_t* incoming_protocol;

        // send queue to queue all further send invocations
        std::shared_ptr<queue::send_t> send_queue;
    };

    auto execute(std::shared_ptr<session_t> session, const operation_t& op) -> void;

    std::deque<operation_t> m_operations;
    synchronized<std::shared_ptr<session_t>> m_session;

};

} // namespace queue
} // namespace vicodyn
} // namespace cocaine
