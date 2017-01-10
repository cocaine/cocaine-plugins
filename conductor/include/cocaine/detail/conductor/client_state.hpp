
#ifndef COCAINE_CONDUCTOR_CLIENT_STATE_HPP
#define COCAINE_CONDUCTOR_CLIENT_STATE_HPP

#include "common.hpp"
#include "client.hpp"

namespace cocaine { namespace isolate { namespace conductor {

namespace state {

class base_t:
    public std::enable_shared_from_this<base_t>
{
public:

    typedef asio::ip::tcp protocol_type;
    typedef session<protocol_type> session_type;

protected:

    std::shared_ptr<client_t> m_parent;

public:

    base_t(std::shared_ptr<client_t> parent);
    
    virtual
    ~base_t() {}
    
    virtual
    void
    enqueue(std::shared_ptr<action::action_t> action) = 0;

    virtual
    void
    cancel(uint64_t request_id) {
        // Empty
    }
    
    virtual
    void
    connect(){
        // Empty
    }

    virtual
    void
    close(){
        // Empty
    }

    virtual
    std::string
    name (){
        return "unknown";
    }
};


class closed_t:
    public base_t
{

public:

    closed_t(std::shared_ptr<client_t> parent);
    
    virtual
    void
    enqueue(std::shared_ptr<action::action_t> action);

    virtual
    void
    cancel(uint64_t request_id);
    
    virtual
    void
    connect();

    virtual
    std::string
    name (){
        return "closed";
    }
};


class connecting_t:
    public base_t
{

    bool m_connecting;
    std::unique_ptr<asio::ip::tcp::socket> m_socket;

public:
    
    connecting_t(std::shared_ptr<client_t> parent);

    virtual
    void
    enqueue(std::shared_ptr<action::action_t> action);

    std::vector<asio::ip::tcp::endpoint>
    resolve(const std::string& hostname, const std::string& port) const;
    
    virtual
    void
    connect();

    virtual
    std::string
    name (){
        return "connecting";
    }

    void
    reschedule_connect();

    void
    on_connect(const std::error_code& ec, std::vector<asio::ip::tcp::endpoint>::const_iterator endpoint);
    //on_connect(const std::error_code& ec, asio::ip::tcp::resolver::iterator endpoint);

    std::shared_ptr<session_type>
    establish_session(std::unique_ptr<asio::ip::tcp::socket> socket);

};


class connected_t:
    public base_t
{
    std::shared_ptr<session_type> m_session;

public:

    connected_t(std::shared_ptr<client_t> parent, std::shared_ptr<session_type> session);

    virtual
    void
    cancel(uint64_t request_id);

    virtual
    void
    enqueue(std::shared_ptr<action::action_t> action);

    virtual
    std::string
    name (){
        return "conected";
    }

    virtual
    void
    close();

};
} // namespace state


}}} // namespace cocaine::isolate::conductor

#endif
