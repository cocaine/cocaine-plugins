
#include "client_state.hpp"

namespace cocaine { namespace isolate { namespace conductor {

namespace state {


base_t::base_t(shared_ptr<client_t> parent):
    m_parent(parent)
{
    // Empty
}



void
closed_t::enqueue(shared_ptr<action::action_t> action){
    m_parent->inbox.push_back(action);
    connect();
}

void
closed_t::cancel(uint64_t request_id){
    // remove action from inbox
    // NOP, since the action will be filtered later in parent->enqueue()
}
    
void
closed_t::connect(){
    if (m_parent->state.get() == this){
        m_parent->state = make_shared<connecting_t>(m_parent);
    }
}


connecting_t::connecting_t (shared_ptr<client_t> parent):
    m_parent(parent)
{
    reschedule_connect();
}

void
connecting_t::enqueue(shared_ptr<action::action_t> action){
    m_parent->inbox.push_back(action);
}
    
void
connecting_t::connect(){
    // Empty
}

void
connecting_t::reschedule_connect(){

    auto resolver asio::ip::tcp::resolver(m_parent->m_loop);
    auto endpoints = resolver.resolve(m_parent->m_loop, tcp::resolver::query("localhost", "6587"));

    m_socket = std::make_unique<asio::ip::tcp::socket>(m_parent->m_loop);

    asio::async_connect(
        *m_socket, endpoints.begin(), endpoints.end(),
        std::bind(&connecting_t::on_connect, shared_from_this(), ph::_1, ph::_2));

}

void
connecting_t::on_connect(const std::error_code& ec, asio::ip::tcp::resolver::iterator endpoint){
    if(ec) {
        COCAINE_LOG_ERROR(m_log, "unable to connect to remote: [%d] %s", ec.value(), ec.message());
            
        m_parent->state = std::make_shared<closed_t>(m_parent);

        return;
            
    } else {
        COCAINE_LOG_DEBUG(m_log, "connected to remote via %s", *endpoint);
    }

    auto session = establish_session(std::move(m_socket));

    m_parent->state = make_shared<connected_t>(m_parent, session);
        
    while(m_parent->inbox.size()){
        auto a = m_parent->inbox.front();
        m_parent->inbox.pop_front();
        m_parent->enqueue(a);
    }
}

shared_ptr<session_type>
connecting_t::establish_session(std::unique_ptr<asio::ip::tcp::socket> socket) {

    std::string remote_endpoint = boost::lexical_cast<std::string>(socket->remote_endpoint());

    auto transport = std::make_unique<io::transport<protocol_type>>(
        std::move(socket)
        );

    transport->socket->set_option(asio::ip::tcp::no_delay(true));

    COCAINE_LOG_DEBUG(log, "established session to endpoint %s", remote_endpoint);

    std::unique_ptr<logging::log_t> log(new logging::log_t(*parent->m_log, {
                { "endpoint", remote_endpoint },
                    { "client",  "conductor" }
            }));

    auto session = std::make_shared<session_type>(std::move(log), std::move(transport), nullptr);
    auto self = shared_from_this();

    parent->m_loop->post([self, session]() { session->pull(); });

    return session;
}





connected_t::connected_t(shared_ptr<client_t> parent, shared_ptr<session_type> session):
    base_t(parent),
    m_session(session)
{
        
}

void
connected_t::cancel(uint64_t request_id){
    COCAINE_LOG_DEBUG (m_parent->log, "cancelling request[%d]", request_id);

    auto requests = m_parent->m_requests_pending;
    auto it = requests->find(request_id);

    if(requests->erase(request_id)){
        COCAINE_LOG_DEBUG (m_parent->log, "request[%d] pending, sending cancellation", request_id);
            
        try {
            m_session->fork(nullptr)->send<io::conductor::cancel>(request_id);
        } catch(const std::system_error& e) {
            COCAINE_LOG_WARNING(m_log, "request[%d] unable to send cancellation: %s",
                                request_id, error::to_string(e));
        }
    }
}

void
connected_t::enqueue(shared_ptr<action::action_t> action){
    m_parent->m_requests_pending[action->id()] = action;
    m_action->send(m_parent->session);
}


} // namespace state


}}} // namespace cocaine::isolate::conductor
