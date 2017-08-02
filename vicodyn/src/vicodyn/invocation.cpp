#include "cocaine/vicodyn/invocation.hpp"

#include "cocaine/vicodyn/stream.hpp"

#include <cocaine/errors.hpp>

namespace cocaine {
namespace vicodyn {

invocation_t::invocation_t(const message_t& incoming_message, const root_t& protocol_root, stream_ptr_t backward_stream) :
    d{incoming_message, protocol_root, std::move(backward_stream)}
{
    auto slot_id = d.incoming_message.type();
    auto protocol_it = d.protocol.find(slot_id);
    if(protocol_it == d.protocol.end()) {
        throw error_t(error::slot_not_found,
                      "could not find event with id {} in protocol for {}", slot_id, name());
    }
    const auto& protocol_tuple = protocol_it->second;
    const auto& forward_protocol = std::get<1>(protocol_tuple);
    if(!forward_protocol) {
        throw error_t(error::slot_not_found,
                      "logical error - initial event is recurrent for span {}, dispatch {}", slot_id, name());
    }

    const auto& backward_protocol = std::get<2>(protocol_tuple);
    if(!backward_protocol) {
        throw error_t(error::slot_not_found,
                      "logical error - backward initial event is recurrent for span {}, dispatch {}", slot_id, name());
    }
}

} // namespace vicodyn
} // namespace cocaine
