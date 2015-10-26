
#ifndef COCAINE_CONDUCTOR_ACTION_HPP
#define COCAINE_CONDUCTOR_ACTION_HPP


namespace cocaine { namespace isolate { namespace conductor {

namespace action {

struct cancellation_t:
    public api::cancellation_t
{
    typedef std::shared_ptr<action_t> parent_ptr;
    typedef std::weak_ptr<action_t> parent_wptr;

    parent_wptr m_parent;

    cancellation_t(parent_ptr parent);

    virtual void cancel();
};


class action_t:
    public enable_shared_from_this<action_t>
{
    // enum states {
    //     st_pending = 0,
    //     st_done,
    //     st_cancelled
    // };

protected:

    uint64_t             m_request_id;
    shared_ptr<client_t> m_parent;

    shared_ptr<io::basic_dispatch_t> m_dispatch;

public:

    struct states {
        const static int st_pending = 0;
        const static int st_done = 1;
        const static int st_cancelled = 2;
    };

    std::atomic<int>     m_state;


    typedef session<asio::ip::tcp> session_type;

    action_t(shared_ptr<client_t> client):
        m_state(states::st_pending),
        m_request_id(++client->max_request_id),
        m_parent(client)
    {}

    template<class Derived>
    shared_ptr<Derived>
    shared_self(){
        return std::static_pointer_cast<Derived>(shared_from_this());
    }

    virtual
    shared_ptr<io::basic_dispatch_t>
    dispatch() = 0;

    virtual void send(shared_ptr<session_type> session) = 0;

    virtual void on_done(const dynamic_t& result) = 0;

    virtual void on_error(const std::error_code& ec) = 0;

    uint64_t
    id(){
        return m_request_id;
    }

    virtual
    auto cancellation() -> std::unique_ptr<api::cancellation_t> {
        std::unique_ptr<api::cancellation_t> c(new cancellation_t(shared_from_this()));
        return std::move(c);
    }

    virtual
    void
    run(){
        m_parent->enqueue(this->shared_from_this());
    }

    virtual
    void
    reset(){
        m_dispatch.reset();
    }

    virtual
    void
    cancel(){
        COCAINE_LOG_DEBUG(m_parent->m_log, "request[%d]: cancelling", m_request_id);
        int expected = states::st_pending;
        if(m_state.compare_exchange_strong(expected, states::st_cancelled)){
            on_cancel();
        } else {
            COCAINE_LOG_WARNING(m_parent->m_log, "request %d cancelled after completion", m_request_id);
        }
    }

    virtual
    void
    done(const dynamic_t& result){
        COCAINE_LOG_DEBUG(m_parent->m_log, "request[%d]: complete", m_request_id);
        int expected = states::st_pending;
        if(m_state.compare_exchange_strong(expected, states::st_done)){
            on_done(result);
        } else {
            COCAINE_LOG_INFO(m_parent->m_log, "request %d completed after cancellation", m_request_id);
        }
    }

    virtual
    void
    error(std::error_code& ec){
        COCAINE_LOG_DEBUG(m_parent->m_log, "request[%d]: error", m_request_id);
        int expected = states::st_pending;
        if(m_state.compare_exchange_strong(expected, states::st_done)){
            on_error(ec);
        } else {
            COCAINE_LOG_INFO(m_parent->m_log, "request %d completed with error after cancellation", m_request_id);
        }
    }

    virtual
    void
    on_cancel(){
        auto self = this->shared_from_this();
        m_parent->post([self](){
            self->m_parent->cancel(self->m_request_id);
        });
    }

    virtual
    ~action_t(){
        // Empty
    }
    
};



template<typename Tag>
class action_dispatch:
    public dispatch<Tag>
{
    
    typedef typename io::protocol<Tag>::scope action_protocol;

    shared_ptr<action_t> m_parent_action;

public:

    action_dispatch(shared_ptr<action_t> parent_action):
        dispatch<Tag>("<unknown"),
        m_parent_action (parent_action)
    {
        typedef typename action_protocol::value value;
        this->template on<typename action_protocol::value>(std::bind(&action_dispatch<Tag>::on_value, this, ph::_1));
        this->template on<typename action_protocol::error>(std::bind(&action_dispatch<Tag>::on_error, this, ph::_1, ph::_2));
    }
    
    void
    discard(const std::error_code& ec){
        if (ec){
            m_parent_action->reset();
        }
    }

    void
    on_value(const dynamic_t& result){
        m_parent_action->done(result);
    }

    void
    on_error(const std::error_code& ec, const std::string& message){
        auto ec1 = const_cast<std::error_code&>(ec);
        m_parent_action->error(ec1);
    }
    
};


} // namespace action


}}} // namespace cocaine::isolate::conductor

#endif
