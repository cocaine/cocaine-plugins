#include "cocaine/vicodyn/queue/invocation.hpp"

#include "cocaine/vicodyn/proxy/dispatch.hpp"
#include "cocaine/vicodyn/stream.hpp"

#include <cocaine/rpc/asio/decoder.hpp>
#include <cocaine/rpc/upstream.hpp>
#include <cocaine/rpc/session.hpp>
#include <cocaine/rpc/graph.hpp>

namespace cocaine {
namespace vicodyn {
namespace queue {

auto invocation_t::absorb(invocation_t&& other) -> void {
    m_session.apply([&](std::shared_ptr<session_t>& session) {
        if(!session) {
            for(auto& op: other.m_operations) {
                m_operations.push_back(std::move(op));
            }
        } else {
            for (auto it = other.m_operations.begin(); it != other.m_operations.end();) {
                try {
                    execute(session, *it);
                    it = other.m_operations.erase(it);
                } catch (...) {
                    it = other.m_operations.erase(it);
                    session = nullptr;
                    for(auto& op: other.m_operations) {
                        m_operations.push_back(std::move(op));
                    }
                    break;
                }
            }
        }
    });
}

auto invocation_t::append(const msgpack::object& message,
                          uint64_t event_id,
                          hpack::header_storage_t headers,
                          const io::graph_node_t& incoming_protocol,
                          stream_ptr_t backward_stream) -> std::shared_ptr<stream_t>
{
    auto forward_stream = std::make_shared<stream_t>(stream_t::direction_t::forward);
    m_session.apply([&](std::shared_ptr<session_t>& session) mutable {
        if(!session) {
            m_operations.resize(m_operations.size()+1);
            auto& op = m_operations.back();

            op.event_id = event_id;
            op.headers = std::move(headers);
            op.forward_stream = forward_stream;
            op.backward_stream = std::move(backward_stream);
            op.zone = std::make_unique<msgpack::zone>();
            op.incoming_protocol = &incoming_protocol;

            msgpack::packer<io::aux::encoded_message_t> packer(op.encoded_message);
            packer << message;
            size_t offset;
            msgpack::unpack(op.encoded_message.data(), op.encoded_message.size(), &offset, op.zone.get(), &op.data);
        } else {
            try {
                backward_stream->attach(session);
                auto backward_dispatch = std::make_shared<proxy::dispatch_t>("<-", std::move(backward_stream), incoming_protocol);
                forward_stream->attach(session->get().fork(backward_dispatch));
                forward_stream->attach(session);
                forward_stream->append(message, event_id, std::move(headers));
            } catch (const std::exception& e) {
                VICODYN_DEBUG("append to invocation queue failed: {}", e.what());
                session = nullptr;
                throw;
            }
        }
    });
    return forward_stream;
}

auto invocation_t::attach(std::shared_ptr<session_t> new_session) -> void {
    m_session.apply([&](std::shared_ptr<session_t>& session){
        BOOST_ASSERT(!session);
        session = std::move(new_session);
        for (auto it = m_operations.begin(); it != m_operations.end();) {
            try {
                execute(session, *it);
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

auto invocation_t::execute(std::shared_ptr<session_t> session, const operation_t& operation) -> void{
    const auto& name = std::get<0>(operation.incoming_protocol->at(operation.event_id));

    auto dispatch = std::make_shared<proxy::dispatch_t>(name, operation.backward_stream,
                                                        *operation.incoming_protocol);

    operation.backward_stream->attach(session);
    operation.forward_stream->attach(session);
    operation.forward_stream->attach(session->get().fork(dispatch));
    operation.forward_stream->append(operation.data, operation.event_id, std::move(operation.headers));
}

auto invocation_t::connected() -> bool {
    return m_session->get() != nullptr;
}

} // namespace queue
} // namespace vicodyn
} // namespace cocaine
