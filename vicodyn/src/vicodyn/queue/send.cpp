#include "cocaine/vicodyn/queue/send.hpp"

#include <cocaine/rpc/asio/encoder.hpp>
#include <cocaine/rpc/asio/decoder.hpp>
#include <cocaine/vicodyn/stream/wrapper.hpp>

namespace cocaine {
namespace vicodyn {
namespace queue {

auto send_t::append(const msgpack::object& message, uint64_t event_id, hpack::header_storage_t headers) -> void {
    //TODO: synchronization
    if(!upstream) {
        operations.resize(operations.size() + 1);
        auto& operation = operations.back();
        operation.event_id = event_id;
        operation.headers = std::move(headers);

        msgpack::packer<io::aux::encoded_message_t> packer(operation.encoded_message);
        packer << message;
        size_t offset;
        msgpack::unpack(operation.encoded_message.data(),
                        operation.encoded_message.size(),
                        &offset,
                        operation.zone.get(),
                        &operation.data);
    } else {
        stream::wrapper_t wrapper(upstream);
        wrapper.append(message, event_id, std::move(headers));
    }
}

auto send_t::attach(io::upstream_ptr_t _upstream) -> void {
    upstream = std::move(_upstream);
    if(!operations.empty()) {
        stream::wrapper_t wrapper(upstream);
        for (auto& operation : operations) {
            wrapper.append(operation.data,
                           operation.event_id,
                           std::move(operation.headers));
        }
        operations.clear();
    }
}

} // namespace queue
} // namespace vicodyn
} // namespace cocaine
