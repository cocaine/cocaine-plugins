#include "cocaine/vicodyn/stream/wrapper.hpp"

#include <cocaine/rpc/asio/decoder.hpp>
#include <cocaine/rpc/asio/encoder.hpp>
#include <cocaine/rpc/upstream.hpp>

namespace cocaine {
namespace vicodyn {
namespace stream {

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

wrapper_t::wrapper_t(io::upstream_ptr_t _upstream) :
    upstream(std::move(_upstream))
{
}

auto wrapper_t::append(const msgpack::object& message,
                       uint64_t event_id,
                       hpack::header_storage_t headers) -> void {
    partially_encoded_t partially_encoded(message, event_id, std::move(headers), upstream->channel_id());
    io::aux::unbound_message_t unbound_message(std::move(partially_encoded));
    VICODYN_DEBUG("sending to upstream with channel {}", upstream->channel_id());
    upstream->send(std::move(unbound_message));
}


} // namespace stream
} // namespace vicodyn
} // namespace cocaine
