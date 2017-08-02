#include "cocaine/vicodyn/proxy.hpp"

#include "cocaine/vicodyn/invocation.hpp"
#include "cocaine/vicodyn/peer.hpp"
#include "cocaine/vicodyn/proxy/dispatch.hpp"
#include "cocaine/vicodyn/stream.hpp"

#include <cocaine/context.hpp>
#include <cocaine/dynamic.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/format.hpp>
#include <cocaine/logging.hpp>

#include <blackhole/logger.hpp>

namespace cocaine {
namespace vicodyn {

proxy_t::proxy_t(context_t& context,
                 const std::string& _name,
                 const dynamic_t& args,
                 unsigned int _version,
                 io::graph_root_t _protocol) :
    io::basic_dispatch_t(_name),
    executor(),
    logger(context.log(_name)),
    m_protocol(_protocol),
    m_version(_version),
    // TODO: Note here we use acceptor io_loop.
    pool(context, executor.asio(), _name, args)
{
    COCAINE_LOG_DEBUG(logger, "created proxy for {}", _name);
}

boost::optional<io::dispatch_ptr_t>
proxy_t::process(const io::decoder_t::message_type& incoming_message, const io::upstream_ptr_t& raw_backward_stream) {
    stream_ptr_t backward_stream = std::make_shared<stream_t>(stream_t::direction_t::backward);
    backward_stream->attach(std::move(raw_backward_stream));
    invocation_t invocation(incoming_message, m_protocol, std::move(backward_stream));

    auto forward_stream = pool.invoke(std::move(invocation));

    // terminal transition
    if(invocation.forward_protocol()->empty()) {
        return boost::optional<io::dispatch_ptr_t>(nullptr);
    }
    auto dispatch_name = cocaine::format("->/{}/{}", name(), invocation.name());
    auto forward_dispatch = std::make_shared<proxy::dispatch_t>(
            std::move(dispatch_name), forward_stream, *invocation.forward_protocol());
    return boost::optional<io::dispatch_ptr_t>(std::move(forward_dispatch));
}

} // namespace vicodyn
} // namespace cocaine
