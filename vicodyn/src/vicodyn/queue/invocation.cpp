#include "cocaine/vicodyn/queue/invocation.hpp"

#include "cocaine/vicodyn/invocation.hpp"
#include "cocaine/vicodyn/proxy/dispatch.hpp"
#include "cocaine/vicodyn/stream.hpp"

#include <cocaine/rpc/asio/decoder.hpp>
#include <cocaine/rpc/upstream.hpp>
#include <cocaine/rpc/session.hpp>
#include <cocaine/rpc/graph.hpp>


namespace cocaine {
namespace vicodyn {
namespace queue {

auto invocation_t::absorb(invocation_t& other) -> void {
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
                    other.m_operations.clear();
                    break;
                }
            }
        }
    });
}

auto invocation_t::append(invocation_request_t invocation) -> std::shared_ptr<stream_t> {
    auto forward_stream = std::make_shared<stream_t>(stream_t::direction_t::forward);

    operation_t operation = operation_t();
    operation.event_id = invocation.message().type();
    operation.headers = std::move(invocation.message().headers());
    operation.forward_stream = forward_stream;
    operation.backward_stream = invocation.backward_stream();
    operation.backward_protocol = invocation.backward_protocol();
    operation.invocation_name = invocation.name();

    m_session.apply([&](std::shared_ptr<session_t>& session) mutable {
        if(!session) {
            operation.zone = std::make_unique<msgpack::zone>();
            msgpack::packer<io::aux::encoded_message_t> packer(operation.encoded_message);
            packer << invocation.message().args();
        } else {
            const auto& args = invocation.message().args();
            operation.data = &args;
            execute(session, operation);
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
                msgpack::object object;
                size_t offset;
                msgpack::unpack(it->encoded_message.data(), it->encoded_message.size(), &offset, it->zone.get(), &object);
                it->data = &object;
                execute(session, *it);
                it = m_operations.erase(it);
            } catch (...) {
                it = m_operations.erase(it);
                session = nullptr;
                throw;
            }
        }
        m_operations.clear();
    });
}

auto invocation_t::disconnect() -> void {
    /// This one is performed in a gracefull way
    /// just resetting pointer guarantee all invocation in progress will be completed
    m_session.apply([&](std::shared_ptr<session_t>& session){
        session = nullptr;
    });
}

auto invocation_t::execute(std::shared_ptr<session_t> session, const operation_t& operation) -> void{
    auto dispatch = std::make_shared<proxy::dispatch_t>(operation.invocation_name, operation.backward_stream,
                                                        *operation.backward_protocol);

    operation.backward_stream->attach(session);
    operation.forward_stream->attach(session);
    operation.forward_stream->attach(session->get().fork(dispatch));
    operation.forward_stream->append(*operation.data, operation.event_id, std::move(operation.headers));
}

auto invocation_t::connected() -> bool {
    return m_session->get() != nullptr;
}

} // namespace queue
} // namespace vicodyn
} // namespace cocaine
