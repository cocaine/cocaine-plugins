#include "cocaine/vicodyn/proxy.hpp"

#include "cocaine/vicodyn/peer.hpp"
#include "cocaine/vicodyn/proxy/dispatch.hpp"
#include "cocaine/vicodyn/stream.hpp"

#include <cocaine/context.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/format.hpp>
#include <cocaine/logging.hpp>

#include <blackhole/logger.hpp>

namespace cocaine {
namespace vicodyn {

proxy_t::proxy_t(context_t& context,
                 std::shared_ptr<asio::io_service> _io_loop,
                 const std::string& _name,
                 const dynamic_t& /*args*/,
                 unsigned int _version,
                 io::graph_root_t _protocol) :
    io::basic_dispatch_t(_name),
    io_loop(std::move(_io_loop)),
    logger(context.log(_name)),
    m_protocol(_protocol),
    m_version(_version),
    // TODO: Note here we use acceptor io_loop.
    pool(api::peer::pool(context, *io_loop, "basic", _name))
{
    VICODYN_DEBUG("create proxy");
}

boost::optional<io::dispatch_ptr_t>
proxy_t::process(const io::decoder_t::message_type& incoming_message, const io::upstream_ptr_t& raw_backward_stream) const {
    auto slot_id = incoming_message.type();
    auto protocol_it = m_protocol.find(slot_id);
    COCAINE_LOG_DEBUG(logger, "graph has {} handles", m_protocol.size());
    COCAINE_LOG_DEBUG(logger, "graph handle is {}", m_protocol.begin()->first);
    if(protocol_it == m_protocol.end()) {
        auto msg = cocaine::format("could not find event with id {} in protocol for {}", slot_id, name());
        COCAINE_LOG_ERROR(logger, msg);
        throw error_t(error::slot_not_found, msg);
    }
    const auto& protocol_tuple = protocol_it->second;
    const auto& forward_protocol = std::get<1>(protocol_tuple);
    if(!forward_protocol) {
        auto msg = cocaine::format("logical error - initial event is recurrent for span {}, dispatch {}", slot_id, name());
        COCAINE_LOG_ERROR(logger, msg);
        throw error_t(error::slot_not_found, msg);
    }

    const auto& backward_protocol = std::get<2>(protocol_tuple);
    if(!backward_protocol) {
        auto msg = cocaine::format("logical error - backward initial event is recurrent for span {}, dispatch {}", slot_id, name());
        COCAINE_LOG_ERROR(logger, msg);
        throw error_t(error::slot_not_found, msg);
    }
    stream_ptr_t backward_stream = std::make_shared<stream_t>(stream_t::direction_t::backward);
    backward_stream->attach(std::move(raw_backward_stream));
    auto forward_stream = pool->invoke(incoming_message, *backward_protocol, std::move(backward_stream));

    // terminal transition
    if(forward_protocol->empty()) {
        return boost::optional<io::dispatch_ptr_t>(nullptr);
    }
    auto dispatch_name = cocaine::format("->/{}/{}", name(), std::get<0>(protocol_tuple));
    auto forward_dispatch = std::make_shared<proxy::dispatch_t>(
            std::move(dispatch_name), forward_stream, *forward_protocol);
    return boost::optional<io::dispatch_ptr_t>(std::move(forward_dispatch));
}

} // namespace vicodyn
} // namespace cocaine
