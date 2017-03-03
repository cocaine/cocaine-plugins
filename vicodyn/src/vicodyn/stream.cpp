#include "cocaine/vicodyn/stream.hpp"

#include <cocaine/rpc/asio/encoder.hpp>
#include <cocaine/rpc/asio/decoder.hpp>
#include <cocaine/rpc/upstream.hpp>

namespace cocaine {
namespace vicodyn {

namespace {

struct partially_encoded_t {
    partially_encoded_t(const msgpack::object& message,
                        uint64_t event_id,
                        hpack::header_storage_t _headers,
                        uint64_t channel_id) :
            encoded_message(new io::aux::encoded_message_t()),
            packer(encoded_message->buffer),
            //TODO: If the performance is really critical we can try to move headers here, not copy
            headers(std::move(_headers))
    {
        packer.pack_array(4);
        packer.pack(channel_id);
        packer.pack(event_id);
        packer.pack(message);
        //skip packing headers till last moment due to statefull encoder
    }

    io::aux::encoded_message_t
    operator()(io::encoder_t& encoder) {
        VICODYN_DEBUG("partially encoded called");
        encoder.pack_headers(packer, headers);
        return std::move(*encoded_message);
    }

    //TODO: FUCK! It's copied by std::function!
    std::shared_ptr<io::aux::encoded_message_t> encoded_message;
    msgpack::packer<io::aux::encoded_buffers_t> packer;
    hpack::header_storage_t headers;
};

}

stream_t::stream_t() { }

auto stream_t::append(const msgpack::object& message, uint64_t event_id, hpack::header_storage_t headers) -> void {
    //TODO: synchronization
    if(!wrapped_stream) {
        operations.resize(operations.size() + 1);
        auto& operation = operations.back();
        operation.event_id = event_id;
        operation.headers = std::move(headers);
        operation.zone.reset(new msgpack::zone);

        msgpack::packer<io::aux::encoded_message_t> packer(operation.encoded_message);
        packer << message;
        size_t offset;
        msgpack::unpack(operation.encoded_message.data(),
                        operation.encoded_message.size(),
                        &offset,
                        operation.zone.get(),
                        &operation.data);
    } else {
        partially_encoded_t partially_encoded(message, event_id, std::move(headers), wrapped_stream->channel_id());
        io::aux::unbound_message_t unbound_message(std::move(partially_encoded));
        VICODYN_DEBUG("sending to stream with channel {}", wrapped_stream->channel_id());
        wrapped_stream->send(std::move(unbound_message));
    }
}

auto stream_t::attach(io::upstream_ptr_t stream) -> void {
    wrapped_stream = std::move(stream);
    if(!operations.empty()) {
        for (auto& op : operations) {
            partially_encoded_t partially_encoded(op.data, op.event_id, std::move(op.headers), wrapped_stream->channel_id());
            io::aux::unbound_message_t unbound_message(std::move(partially_encoded));
            VICODYN_DEBUG("sending to stream with channel {}", wrapped_stream->channel_id());
            wrapped_stream->send(std::move(unbound_message));
        }
        operations.clear();
    }
}

auto stream_t::attach(std::shared_ptr<session_t> _session) -> void {
    session = _session;
}

} // namespace vicodyn
} // namespace cocaine
