#include "cocaine/vicodyn/proxy/dispatch.hpp"

#include "cocaine/vicodyn/proxy.hpp"


#include <cocaine/errors.hpp>
#include <cocaine/format.hpp>


namespace cocaine {
namespace vicodyn {
namespace proxy {

auto make_root(const io::graph_node_t& branch) -> io::graph_root_t {
    io::graph_root_t root;
    io::graph_node_t reverse_protocol;
    for(auto p: branch) {
        root[p.first] = std::make_tuple(std::get<0>(p.second), std::get<1>(p.second), boost::make_optional(reverse_protocol));
    }
    return root;
}

dispatch_t::dispatch_t(const std::string& name,
                       std::shared_ptr<stream_t> _proxy_stream,
                       const io::graph_node_t& _current_state) :
    io::basic_dispatch_t(name),
    proxy_stream(std::move(_proxy_stream)),
    full_name(name),
    current_state(&_current_state),
    current_root(make_root(*current_state))
{
}

auto dispatch_t::root() const -> const io::graph_root_t& {
    //TODO: What can we do here except passing parent everywhere or generating graph_root on the fly?
    return current_root;
}

auto dispatch_t::version() const -> int {
    //TODO: What can we do here except passing parent everywhere?
    return 1;
}


auto dispatch_t::process(const io::decoder_t::message_type& incoming_message, const io::upstream_ptr_t&) const
    -> boost::optional<io::dispatch_ptr_t>
{
    VICODYN_DEBUG("processing dispatch {}/{}", incoming_message.span(), incoming_message.type());
    proxy_stream->append(incoming_message.args(), incoming_message.type(), incoming_message.headers());
    auto event_span = incoming_message.type();
    auto protocol_span = current_state->find(event_span);
    if(protocol_span == current_state->end()) {
        auto msg = cocaine::format("could not find event with id {} in protocol for {}", event_span, name());
        //COCAINE_LOG_ERROR(logger, msg);
        throw error_t(error::slot_not_found, msg);
    }
    auto protocol_tuple = protocol_span->second;
    const auto& incoming_protocol = std::get<1>(protocol_tuple);

    //recurrent transition
    if(!incoming_protocol) {
        return boost::none;
    }

    // terminal transition
    if(incoming_protocol->empty()) {
        return boost::optional<io::dispatch_ptr_t>(nullptr);
    }

    //next transition
    current_state = &(*incoming_protocol);
    current_root = make_root(*current_state);

    full_name = cocaine::format("{}/{}", full_name, std::get<0>(protocol_tuple));
    return boost::optional<io::dispatch_ptr_t>(shared_from_this());
}

auto dispatch_t::discard(const std::error_code& ec) const -> void {
    proxy_stream->discard(ec);
}

}
}
} // namesapce cocaine
