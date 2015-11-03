
#ifndef COCAINE_CONDUCTOR_ACTIONS_HPP
#define COCAINE_CONDUCTOR_ACTIONS_HPP

#include "action.hpp"


namespace cocaine { namespace isolate { namespace conductor {

namespace action {

typedef std::function<void(const std::error_code&)> spool_handler_type;
typedef std::function<void(const std::error_code&, unique_ptr<api::handle_t>&)> spawn_handler_type;
typedef std::function<void()> terminate_handler_type;

class spool_t:
    public action_t
{
    typedef io::event_traits<io::conductor::spool>::upstream_type result_tag;
    
    std::string m_name;
    dynamic_t m_profile;
    spool_handler_type m_handler;

public:

    spool_t(shared_ptr<client_t> client, std::string& name, dynamic_t& profile, spool_handler_type handler):
        action_t(client),
        m_name(name),
        m_profile(profile),
        m_handler(handler)
    {}

    virtual
    io::dispatch_ptr_t
    dispatch(){
        BOOST_ASSERT(!m_dispatch);
        m_dispatch = std::make_shared<action_dispatch<result_tag>>(shared_from_this());
        return m_dispatch;
    }

    virtual
    std::string
    name (){
        return std::string("<spool>");
    }

    virtual
    void
    send(shared_ptr<session_type> session){
        if (!m_dispatch){
            dispatch();
        }
        auto downstream = session->fork(m_dispatch);
        downstream->send<io::conductor::spool>(m_request_id, m_name, m_profile);
    }

    virtual
    void
    on_error(const std::error_code& ec){
        auto self = this->shared_from_this();
        m_parent->post([self, ec, this](){
            m_handler(ec);
        });
    }

    virtual
    void
    on_done(const dynamic_t& result){
        auto self = this->shared_from_this();
        m_parent->post([self, this](){
            m_handler(std::error_code());
        });
    }
};

class spawn_t:
    public action_t
{
    typedef io::event_traits<io::conductor::spawn>::upstream_type result_tag;

    std::string        m_name;
    dynamic_t          m_profile;
    std::string        m_path;
    dynamic_t          m_args;
    dynamic_t          m_environment;
    spawn_handler_type m_handler;

public:

    spawn_t(std::shared_ptr<client_t> client,
            std::string name,
            dynamic_t profile,
            std::string path,
            api::string_map_t args,
            api::string_map_t environment,
            spawn_handler_type handler):
        action_t(client),
        m_name(name),
        m_profile(profile),
        m_path(path),
        m_args(args),
        m_environment(environment),
        m_handler(handler)
    {
    }

    virtual
    io::dispatch_ptr_t
    dispatch(){
        BOOST_ASSERT(!m_dispatch);
        m_dispatch = std::make_shared<action_dispatch<result_tag>>(shared_from_this());
        return m_dispatch;
    }

    virtual
    std::string
    name (){
        return std::string("<spawn>");
    }

    virtual
    void
    send(shared_ptr<session_type> session){
        if (!m_dispatch){
            dispatch();
        }
        auto downstream = session->fork(m_dispatch);
        downstream->send<io::conductor::spawn>(
            m_request_id,
            m_name,
            m_profile,
            m_args,
            m_environment
        );
    }

    virtual
    void
    on_error(const std::error_code& ec){
        auto self = this->shared_from_this();
        std::unique_ptr<api::handle_t> handle;
        m_parent->post(
            cocaine::detail::move_handler(std::bind(m_handler, ec, std::move(handle)))
        );
    }

    virtual
    void
    on_done(const dynamic_t& result){

        auto r = result.as_object();
        auto container_id = r.at("container_id").as_string();
        auto stdout_path = r.at("stdout_path").as_string();

        auto container = make_shared<container_t>(
            m_parent,
            container_id,
            m_name,
            m_profile,
            stdout_path);
        auto self = shared_from_this();
        
        auto handle = container->handle();

        m_parent->m_loop.post (
            cocaine::detail::move_handler(std::bind(m_handler, std::error_code(), std::move(handle)))
        );
    }
};


class terminate_t:
    public action_t
{
    typedef io::event_traits<io::conductor::terminate>::upstream_type result_tag;

    std::string m_name;
    dynamic_t   m_profile;
    std::string m_container_id;

public:

    terminate_t(std::shared_ptr<client_t> client,
                std::string container_id,
                std::string name,
                dynamic_t profile):
        action_t(client),
        m_container_id(container_id),
        m_name(name),
        m_profile(profile)
    {
    }

    virtual
    io::dispatch_ptr_t
    dispatch (){
        BOOST_ASSERT(!m_dispatch);
        m_dispatch = std::make_shared<action_dispatch<result_tag>>(shared_from_this());
        return m_dispatch;
    }

    virtual
    std::string
    name (){
        return std::string("<terminate>");
    }

    virtual
    void
    send(shared_ptr<session_type> session){
        if (!m_dispatch){
            dispatch();
        }
        auto downstream = session->fork(m_dispatch);
        downstream->send<io::conductor::terminate>(m_request_id, m_container_id);
    }

    virtual
    void
    on_error(const std::error_code& ec){
        // Empty
    }

    virtual
    void
    on_done(const dynamic_t& result){
        // Empty
    }

};

} // namespace action


}}} // namespace cocaine::isolate::conductor

#endif
