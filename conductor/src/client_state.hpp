
#ifndef COCAINE_CONDUCTOR_CLIENT_STATE_HPP
#define COCAINE_CONDUCTOR_CLIENT_STATE_HPP


namespace cocaine { namespace isolate { namespace conductor {

namespace state {

class base_t:
    public enable_shared_from_this<base_t>
{
public:

    typedef asio::ip::tcp protocol_type;
    typedef session<protocol_type> session_type;

protected:

    shared_ptr<client_t> m_parent;

public:

    base_t(shared_ptr<client_t> parent);
    
    virtual
    ~base_t() {};
    
    virtual
    void
    enqueue(shared_ptr<action::action_t> action) = 0;

    virtual
    void
    cancel(uint64_t request_id) = 0;
    
    virtual
    void
    connect() = 0;

};


class closed_t:
    public base_t
{

public:

    closed_t(shared_ptr<client_t> parent): base_t(parent) {}
    
    virtual
    void
    enqueue(shared_ptr<action::action_t> action);

    virtual
    void
    cancel(uint64_t request_id);
    
    virtual
    void
    connect();
};


class connecting_t:
    public base_t
{

    std::unique_ptr<asio::ip::tcp::socket> m_socket;

public:
    
    connecting_t (shared_ptr<client_t> parent);

    virtual
    void
    enqueue(shared_ptr<action::action_t> action);
    
    virtual
    void
    connect();

    void
    reschedule_connect();

    void
    on_connect(const std::error_code& ec, asio::ip::tcp::resolver::iterator endpoint);

    shared_ptr<session_type>
    establish_session(std::unique_ptr<asio::ip::tcp::socket> socket);

};


class connected_t:
    public base_t
{
    shared_ptr<session_type> m_session;

public:

    connected_t(shared_ptr<client_t> parent, shared_ptr<session_type> session):
        base_t(parent),
        m_session(session)
    {
        
    }

    virtual
    void
    cancel(uint64_t request_id);

    virtual
    void
    enqueue(shared_ptr<action::action_t> action);

};
} // namespace state


}}} // namespace cocaine::isolate::conductor

#endif
