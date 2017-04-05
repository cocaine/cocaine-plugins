#include "cocaine/vicodyn/stream.hpp"

#include "cocaine/vicodyn/session.hpp"

#include <cocaine/idl/primitive.hpp>
#include <cocaine/rpc/asio/encoder.hpp>
#include <cocaine/rpc/asio/decoder.hpp>
#include <cocaine/rpc/upstream.hpp>
#include <cocaine/rpc/dispatch.hpp>

namespace cocaine {
namespace vicodyn {

namespace {

struct discard_dispatch_t: public dispatch<io::option_of<>::tag> {
    discard_dispatch_t(std::shared_ptr<session_t> _session) :
            dispatch<io::option_of<>::tag>(cocaine::format("{}/discarder", _session->get().name())),
            session(std::move(_session))
    {
        on<io::protocol<io::option_of<>::tag>::scope::value>([](){
            VICODYN_DEBUG("channel sucessfully discarded");
        });
        on<io::protocol<io::option_of<>::tag>::scope::error>([&](const std::error_code& ec, const std::string& msg){
            VICODYN_DEBUG("can not discard channel, terminating session - ({}){}", ec.value(), msg);
            session->get().detach(ec);
        });
    }

    std::shared_ptr<session_t> session;
};

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
        encoder.pack_headers(packer, headers);
        return std::move(*encoded_message);
    }

    //TODO: FUCK! It's copied by std::function!
    std::shared_ptr<io::aux::encoded_message_t> encoded_message;
    msgpack::packer<io::aux::encoded_buffers_t> packer;
    hpack::header_storage_t headers;
};

}

stream_t::stream_t(direction_t _direction) : direction(_direction){ }

auto stream_t::append(const msgpack::object& message, uint64_t event_id, hpack::header_storage_t headers) -> void {
    //TODO: can we decrease time under mutex?
    attach_mutex.apply([&]{
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
            wrapped_stream->send(std::move(unbound_message));
        }
    });
}

auto stream_t::attach(io::upstream_ptr_t stream) -> void {
    attach_mutex.apply([&]{
        wrapped_stream = std::move(stream);
        if(!operations.empty()) {
            for (auto& op : operations) {
                partially_encoded_t partially_encoded(op.data, op.event_id, std::move(op.headers), wrapped_stream->channel_id());
                io::aux::unbound_message_t unbound_message(std::move(partially_encoded));
                wrapped_stream->send(std::move(unbound_message));
            }
            operations.clear();
        }
        try_discard();
    });

}

auto stream_t::attach(std::shared_ptr<session_t> _session) -> void {
    attach_mutex.apply([&]{
        session = _session;
        try_discard();
    });
}

auto stream_t::discard(std::error_code ec) -> void {
    VICODYN_DEBUG("discard called");
    attach_mutex.apply([&]{
        discard_code = std::move(ec);
        try_discard();
    });
}

auto stream_t::try_discard() -> void {
    if(!discard_code) {
        return;
    }

    if(direction == direction_t::forward && session) {
        auto upstream = session->get().fork(std::make_shared<discard_dispatch_t>(session));
        try {
            upstream->send<io::control::revoke>(wrapped_stream->channel_id(), *discard_code);
        } catch(...) {
            // session is gone - no need to revoke channel
        }
        discard_code = boost::none;
    }
    if(direction == direction_t::backward && wrapped_stream) {
        wrapped_stream->detach_session(*discard_code);
        discard_code = boost::none;
    }
}

} // namespace vicodyn
} // namespace cocaine
