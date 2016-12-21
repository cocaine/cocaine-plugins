#include "cocaine/vicodyn/queue/invocation.hpp"

#include <cocaine/rpc/asio/decoder.hpp>
#include <cocaine/rpc/upstream.hpp>
#include <cocaine/rpc/session.hpp>
#include <cocaine/vicodyn/stream/wrapper.hpp>
#include <cocaine/rpc/graph.hpp>
#include <cocaine/vicodyn/proxy/dispatch.hpp>
#include <cocaine/vicodyn/queue/send.hpp>

namespace cocaine {
namespace vicodyn {
namespace queue {

auto invocation_t::append(const msgpack::object& message,
                          uint64_t event_id,
                          hpack::header_storage_t headers,
                          const io::graph_node_t& incoming_protocol,
                          io::upstream_ptr_t upstream) -> std::shared_ptr<queue::send_t>
{
    auto send_queue = m_session.apply([&](std::shared_ptr<session_t>& session) -> std::shared_ptr<queue::send_t> {
        if(!session) {
            return nullptr;
        }
        try {
            const auto& name = std::get<0>(incoming_protocol.at(event_id));

            auto upstream_wrapper = std::make_shared<stream::wrapper_t>(upstream);
            auto dispatch = std::make_shared<proxy::dispatch_t>(name, upstream_wrapper, incoming_protocol);

            auto downstream = session->fork(dispatch);
            stream::wrapper_t downstream_wrapper(downstream);
            downstream_wrapper.append(message, event_id, std::move(headers));

            auto s_queue = std::make_shared<queue::send_t>();
            s_queue->attach(std::move(downstream));

            return s_queue;
        } catch (const std::exception& e) {
            VICODYN_DEBUG("append to invocation queue failed: {}", e.what());
            session = nullptr;
            throw;
        }
    });

    if(!send_queue) {
        m_operations.resize(m_operations.size()+1);
        auto& operation = m_operations.back();
        operation.event_id = event_id;
        operation.headers = std::move(headers);
        operation.send_queue = std::make_shared<queue::send_t>();
        operation.upstream = std::move(upstream);
        operation.zone = std::make_unique<msgpack::zone>();
        //TODO: WTF? I'm too tired to read what compiler says to me
        //operation.incoming_protocol = const_cast<io::graph_node_t*>(&incoming_protocol);
        operation.incoming_protocol = &incoming_protocol;
        msgpack::packer<io::aux::encoded_message_t> packer(operation.encoded_message);
        packer << message;
        size_t offset;
        msgpack::unpack(operation.encoded_message.data(),
                        operation.encoded_message.size(),
                        &offset,
                        operation.zone.get(),
                        &operation.data);
        send_queue = operation.send_queue;
    }
    return send_queue;

}

auto invocation_t::attach(std::shared_ptr<session_t> _session) -> void {
    m_session.apply([&](std::shared_ptr<session_t>& session){
        BOOST_ASSERT(!session);
        session = _session;
        for (auto it = m_operations.begin(); it != m_operations.end();) {
            try {
                const auto& operation = *it;
                const auto& name = std::get<0>(operation.incoming_protocol->at(operation.event_id));

                auto upstream_wrapper = std::make_shared<stream::wrapper_t>(operation.upstream);
                auto dispatch = std::make_shared<proxy::dispatch_t>(name, upstream_wrapper,
                                                                    *operation.incoming_protocol);

                auto downstream = session->fork(dispatch);
                stream::wrapper_t downstream_wrapper(downstream);
                downstream_wrapper.append(operation.data, operation.event_id, std::move(operation.headers));

                operation.send_queue->attach(std::move(downstream));
                it = m_operations.erase(it);
            } catch (...) {
                it = m_operations.erase(it);
                session = nullptr;
                throw;
            }
        }
    });

    // safe to clean outside lock as it is not used anymore after session is set
    m_operations.clear();
}

auto invocation_t::connected() -> bool {
    return m_session->get() != nullptr;
}

} // namespace queue
} // namespace vicodyn
} // namespace cocaine
