

namespace cocaine { namespace isolate { namespace conductor {

namespace action {

typedef std::function<void(const std::error_code&)> spool_handler_type;
typedef std::function<void(const std::error_code&, std::unique_ptr<api::handle_t>)> spawn_handler_type;
typedef std::function<void()> terminate_handler_type;

class spool_t:
    public action_t
{
    typedef io::event_traits<io::conductor::spool>::upstream_type result_tag;
    
    spool_handler_type m_handler;

public:

    spool_t(std::shared_ptr<client_t> client, spool_handler_type handler):
        action_t(client),
        m_handler(handler)
    {}

    virtual
    std::shared_ptr<io::basic_dispatch_t>
    dispatch (){
        BOOST_ASSERT(!m_dispatch);
        m_dispatch = std::make_shared<action_dispatch<result_tag>>(shared_from_this());
        return m_dispatch;
    }

    virtual
    void
    send(std::shared_ptr<session_type> session){
        if (!m_dispatch){
            dispatch();
        }
        auto downstream = session->fork(m_dispatch);
        downstream->send<io::conductor::spool>(request_id, env, args);
    }

    virtual
    void
    on_error(std::error_code ec){
        auto self = this->shared_from_this();
        m_parent->post([self, ec](){
            m_handler(ec);
        });
    }

    virtual
    void
    on_done(dynamic_t result){
        auto self = this->shared_from_this();
        m_parent->post([self](){
            m_handler(std::error_code());
        });
    }
};

class spawn_t:
    public action_t
{
    typedef io::event_traits<io::conductor::spawn>::upstream_type result_tag;

    spawn_handler_type m_handler;
    dynamic_t          m_profile;
    std::string        path;
    dynamic_t          m_args;
    dynamic_t          m_environment;

public:

    spawn_t(std::shared_ptr<client_t>,
            std::sring name,
            dynamic_t profile,
            std::string path,
            api::string_map_t args,
            api::string_map_t environment,
            spawn_handler_type handler):
        action_t(client),
        m_handler(handler)
    {
        
    }

    virtual
    std::shared_ptr<io::basic_dispatch_t>
    dispatch(){
        BOOST_ASSERT(!m_dispatch);
        m_dispatch = std::make_shared<action_dispatch<result_tag>>(shared_from_this());
        return m_dispatch;
    }

    virtual
    void
    send(std::shared_ptr<session_type> session) {
        if (!m_dispatch){
            dispatch();
        }
        auto downstream = session->fork(m_dispatch);
        downstream->send<io::conductor::spawn>(request_id, env, args);
    }

    virtual
    std::unique_ptr<api::cancellation_t>
    async_spawn(const std::string& path,
                const api::string_map_t& args,
                const api::string_map_t& environment,
                api::spawn_handler_t handler)
    {

        auto action = std::make_shared<spawn_action_t>(
            m_client,
            m_name,
            m_profile,
            path,
            args,
            environment,
            handler
        );

        return post_action(action);
    }

    virtual
    void
    on_error(std::error_code ec){
        auto self = this->shared_from_this();
        m_parent->post([self, ec](){
            m_handler(ec, std::unique_ptr<handle_t>(nullptr));
        });
    }

    virtual
    void
    on_done(dynamic_t result){

        auto r = result.as_object();
        auto container_id = r.at("container_id").as_string();

        auto container = std::make_shared<container_t>(m_parent, container_id);
        auto self = shared_from_this();
        
        m_parent->post([self, container](){
            m_handler(std::error_code(), container->handle());
        });
    }
};


class terminate_t:
    public action_t
{
    typedef io::event_traits<io::conductor::terminate>::upstream_type result_tag;

public:

    virtual
    std::shared_ptr<io::basic_dispatch_t>
    dispatch (){
        BOOST_ASSERT(!m_dispatch);
        m_dispatch = std::make_shared<action_dispatch<result_tag>>(shared_from_this());
        return m_dispatch;
    }

    virtual
    void
    send(std::shared_ptr<session_type> session){
        if (!m_dispatch){
            dispatch();
        }
        auto downstream = session->fork(m_dispatch);
        downstream->send<io::conductor::terminate>(request_id, env, args);
    }

    virtual
    void
    on_error(std::error_code ec){
        // Empty
    }

    virtual
    void
    on_done(dynamic_t result){
        // Empty
    }

};

} // namespace action


}}} // namespace cocaine::isolate::conductor
