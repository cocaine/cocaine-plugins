#pragma once

#include "cocaine/vicodyn/forwards.hpp"

#include <cocaine/rpc/asio/decoder.hpp>
#include <cocaine/rpc/graph.hpp>

namespace cocaine {
namespace vicodyn {

class invocation_t {
    using message_t = io::decoder_t::message_type;
    using root_t = io::graph_root_t;

    struct {
        const message_t& incoming_message;
        const io::graph_root_t& protocol;
        stream_ptr_t backward_stream;
    } d;

public:
    invocation_t(const message_t& incoming_message, const root_t& protocol_root, stream_ptr_t backward_stream);

    auto protocol() -> const io::graph_root_t& {
        return d.protocol;
    }
    auto backward_protocol() -> const io::graph_node_t* {
        return std::get<2>(d.protocol.at(d.incoming_message.type())).get_ptr();
    }

    auto forward_protocol() -> const io::graph_node_t* {
        return std::get<1>(d.protocol.at(d.incoming_message.type())).get_ptr();
    };

    auto name() -> const std::string& {
        return std::get<0>(d.protocol.at(d.incoming_message.type()));
    }

    auto message() -> const message_t& {
        return d.incoming_message;
    }

    auto backward_stream() -> stream_ptr_t {
        return d.backward_stream;
    }

};

} // namespace vicodyn
} // namespace cocaine
