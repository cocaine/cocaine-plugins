
#include <asio/connect.hpp>

#include "cocaine/detail/conductor/client_state.hpp"
#include "cocaine/detail/conductor/action.hpp"

#include <cocaine/rpc/asio/transport.hpp>


namespace cocaine { namespace isolate { namespace conductor {

namespace state {

using namespace asio;
using namespace asio::ip;

base_t::base_t(std::shared_ptr<client_t> parent):
    m_parent(parent)
{
    // Empty
}

closed_t::closed_t(std::shared_ptr<client_t> parent):
    base_t(parent)
{
}

void
closed_t::enqueue(std::shared_ptr<action::action_t> action){
    m_parent->m_inbox.push_back(action);
    connect();
}

void
closed_t::cancel(uint64_t request_id){
    // remove action from inbox
    // NOP, since the action will be filtered later in parent->enqueue()
}

void
closed_t::connect(){
    if (m_parent->m_state.get() == this){
        auto connecting = std::make_shared<connecting_t>(m_parent);
        m_parent->migrate(shared_from_this(), connecting);
        connecting->connect();
    }
}


connecting_t::connecting_t(std::shared_ptr<client_t> parent):
    base_t(parent),
    m_connecting(false)
{
}

void
connecting_t::enqueue(std::shared_ptr<action::action_t> action) {
    m_parent->m_inbox.push_back(action);
}

void
connecting_t::connect(){
    if(!m_connecting){
        m_connecting = true;
        reschedule_connect();
    }
}

std::vector<tcp::endpoint>
connecting_t::resolve(const std::string& hostname, const std::string& port) const {
    tcp::resolver::iterator begin;

    try {

        const tcp::resolver::query::flags flags = tcp::resolver::query::address_configured
                                                | tcp::resolver::query::numeric_service;

        begin = tcp::resolver(m_parent->m_loop).resolve(
            tcp::resolver::query(hostname, port, flags));

    } catch(const std::system_error& e) {
        COCAINE_LOG_ERROR(m_parent->m_log, "unable to resolve endpoints: %s", error::to_string(e));
        return std::vector<tcp::endpoint>();
    }

    std::vector<tcp::endpoint> endpoints;

    std::transform(
        begin, tcp::resolver::iterator(), std::back_inserter(endpoints),
        std::bind(&tcp::resolver::iterator::value_type::endpoint, std::placeholders::_1));

    return endpoints;
}

void
connecting_t::reschedule_connect(){

    asio::ip::tcp::resolver resolver(m_parent->m_loop);
    asio::ip::tcp::resolver::iterator it, end;

    auto endpoints = resolve("localhost", "6587");

    m_socket = std::make_unique<asio::ip::tcp::socket>(m_parent->m_loop);

    // m_socket->async_connect(
    //     endpoints.begin(), endpoints.end(),
    //     std::bind(&connecting_t::on_connect, shared_from_this(), ph::_1, ph::_2));

    auto self = shared_from_this();

    asio::async_connect(
        *m_socket, endpoints.begin(), endpoints.end(),
        [self, this] (const std::error_code& ec, std::vector<tcp::endpoint>::const_iterator endpoint)
    {
        on_connect(ec, endpoint);
    });
}

void
connecting_t::on_connect(const std::error_code& ec, std::vector<tcp::endpoint>::const_iterator endpoint){
    if(ec) {
        COCAINE_LOG_ERROR(m_parent->m_log, "unable to connect to remote: [%d] %s", ec.value(), ec.message());

        auto closed = std::make_shared<closed_t>(m_parent);
        m_parent->migrate(shared_from_this(), closed);

        return;
            
    } else {
        COCAINE_LOG_DEBUG(m_parent->m_log, "connected to remote via %s", *endpoint);
    }

    auto session = establish_session(std::move(m_socket));

    auto connected = std::make_shared<connected_t>(m_parent, session);
    m_parent->migrate(shared_from_this(), connected);
        
    while(!m_parent->m_inbox.empty()){
        auto a = m_parent->m_inbox.front();
        m_parent->m_inbox.pop_front();
        m_parent->enqueue(a);
    }
}

std::shared_ptr<base_t::session_type>
connecting_t::establish_session(std::unique_ptr<asio::ip::tcp::socket> socket) {

    std::string remote_endpoint = boost::lexical_cast<std::string>(socket->remote_endpoint());

    auto transport = std::make_unique<io::transport<protocol_type>>(
        std::move(socket)
    );

    transport->socket->set_option(asio::ip::tcp::no_delay(true));

    COCAINE_LOG_DEBUG(m_parent->m_log, "established session to endpoint %s", remote_endpoint);

    std::unique_ptr<logging::log_t> log(new logging::log_t(*m_parent->m_log, {
        { "endpoint", remote_endpoint },
        { "client",  "conductor" }
    }));

    auto session = std::make_shared<session_type>(std::move(log), std::move(transport), nullptr);
    auto self = shared_from_this();

    m_parent->m_loop.post([self, session]() { session->pull(); });

    return session;
}


connected_t::connected_t(std::shared_ptr<client_t> parent, std::shared_ptr<session_type> session):
    base_t(parent),
    m_session(session)
{
        
}

void
connected_t::cancel(uint64_t request_id){
    COCAINE_LOG_DEBUG (m_parent->m_log, "cancelling request[%d]", request_id);

    auto requests = m_parent->m_requests_pending;
    auto it = requests.find(request_id);

    if(requests.erase(request_id)){
        COCAINE_LOG_DEBUG(m_parent->m_log, "request[%d] pending, sending cancellation", request_id);
            
        try {
            m_session->fork(nullptr)->send<io::conductor::cancel>(request_id);
        } catch(const std::system_error& e) {
            COCAINE_LOG_WARNING(m_parent->m_log, "request[%d] unable to send cancellation: %s",
                                request_id, error::to_string(e));
        }
    }
}

void
connected_t::enqueue(std::shared_ptr<action::action_t> action){
    m_parent->m_requests_pending[action->id()] = action;
    action->send(m_session);
}


} // namespace state


}}} // namespace cocaine::isolate::conductor
