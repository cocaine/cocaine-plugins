
#ifndef COCAINE_CONDUCTOR_ACTION_HPP
#define COCAINE_CONDUCTOR_ACTION_HPP

#include "common.hpp"
#include "client.hpp"


namespace cocaine { namespace isolate { namespace conductor {

namespace action {

struct cancellation_t:
    public api::cancellation_t
{
    typedef std::shared_ptr<action_t> parent_ptr;
    typedef std::weak_ptr<action_t> parent_wptr;

    parent_wptr m_parent;

    cancellation_t(parent_ptr parent):
        m_parent(parent)
    {}

    virtual void cancel();

};


class action_t:
    public std::enable_shared_from_this<action_t>
{

protected:

    uint64_t             m_request_id;
    std::shared_ptr<client_t> m_parent;

    io::dispatch_ptr_t   m_dispatch;

public:

#define st_pending 0
#define st_done 1
#define st_cancelled 2

    std::atomic<int>     m_state;

    typedef session<asio::ip::tcp> session_type;

    action_t(std::shared_ptr<client_t> client):
        m_state(st_pending),
        m_request_id(++client->max_request_id),
        m_parent(client)
    {
        COCAINE_LOG_DEBUG(m_parent->m_log, "*action_t[%d]", m_request_id);
        // Empty
    }


    virtual
    io::dispatch_ptr_t
    dispatch() = 0;

    virtual
    std::string
    name (){
        return std::string("<action>");
    }

    virtual void send(std::shared_ptr<session_type> session) = 0;

    virtual void on_done(const dynamic_t& result) = 0;

    virtual void on_error(const std::error_code& ec) = 0;

    uint64_t
    id(){
        return m_request_id;
    }

    virtual
    auto cancellation() -> std::unique_ptr<api::cancellation_t> {
        std::unique_ptr<api::cancellation_t> c(new cancellation_t(shared_from_this()));
        return c;
    }

    virtual
    void
    run(){
        m_parent->enqueue(shared_from_this());
    }

    virtual
    void
    disown_dispatch(){
        if (m_dispatch){
            COCAINE_LOG_DEBUG(m_parent->m_log, "request[%d] discarding child dispatch", id());
            //m_dispatch->discard(std::error_code(0, conductor_category()));
            m_dispatch->discard(std::error_code(65418, std::system_category()));
            m_dispatch.reset();
        } else {
            COCAINE_LOG_DEBUG(m_parent->m_log, "request[%d] has null dispatch", id());
        }
    }
    
    virtual
    void
    dismay(const std::error_code& ec){
        COCAINE_LOG_ERROR(m_parent->m_log, "request[%d] closing parent due to [%d] %s", id(),  ec.value(), ec.message());
        disown_dispatch();
        m_parent->close();
    }

    virtual
    void
    cancel(){
        COCAINE_LOG_DEBUG(m_parent->m_log, "request[%d]: cancelling", m_request_id);
        int expected = st_pending;
        if(m_state.compare_exchange_strong(expected, st_cancelled)){
            on_cancel();
        } else {
            COCAINE_LOG_WARNING(m_parent->m_log, "request %d cancelled after completion", m_request_id);
        }
    }

    virtual
    void
    done(const dynamic_t& result){
        COCAINE_LOG_DEBUG(m_parent->m_log, "request[%d]: complete", m_request_id);
        int expected = st_pending;
        if(m_state.compare_exchange_strong(expected, st_done)){
            on_done(result);
        } else {
            COCAINE_LOG_INFO(m_parent->m_log, "request %d completed after cancellation", m_request_id);
        }
    }

    virtual
    void
    error(std::error_code& ec){
        COCAINE_LOG_DEBUG(m_parent->m_log, "request[%d]: error", m_request_id);
        int expected = st_pending;
        if(m_state.compare_exchange_strong(expected, st_done)){
            on_error(ec);
        } else {
            COCAINE_LOG_INFO(m_parent->m_log, "request %d completed with error after cancellation", m_request_id);
        }
    }

    virtual
    void
    on_cancel(){
        auto self = this->shared_from_this();
        m_parent->post([self, this](){
            m_parent->cancel(m_request_id);
        });
    }

    virtual
    ~action_t(){
        COCAINE_LOG_DEBUG(m_parent->m_log, "~action_t[%d]", m_request_id);
    }

    const
    cocaine::logging::log_t&
    log() {
        return *m_parent->m_log;
    }
    
};


template<typename Tag>
class action_dispatch:
    public dispatch<Tag>
{
    
    typedef typename io::protocol<Tag>::scope action_protocol;

    std::shared_ptr<action_t> m_parent_action;

    mutable bool m_disowned;

public:

    action_dispatch(std::shared_ptr<action_t> parent_action):
        dispatch<Tag>(parent_action->name()),
        m_parent_action(parent_action),
        m_disowned(false)
    {
        typedef typename action_protocol::value value;
        this->template on<typename action_protocol::value>(std::bind(&action_dispatch<Tag>::on_value, this, ph::_1));
        this->template on<typename action_protocol::error>(std::bind(&action_dispatch<Tag>::on_error, this, ph::_1, ph::_2));
    }

    virtual
    void
    discard(const std::error_code& ec) const {
        if(ec) {
            if (ec.value() == 65418){
            //if (category() == conductor_category()){
                COCAINE_LOG_DEBUG(m_parent_action->log(), "request[%d]<dispatch> disowned by parent: [%d] %s", m_parent_action->id(), ec.value(), ec.message());
                m_disowned = true;
                return;
            }
            
            if(m_disowned){
                COCAINE_LOG_DEBUG(m_parent_action->log(), "request[%d]<dispatch:disowned> discarded: [%d] %s", m_parent_action->id(), ec.value(), ec.message());
                return;
            }
            COCAINE_LOG_ERROR(m_parent_action->log(), "request[%d]<dispatch> discarded: [%d] %s", m_parent_action->id(), ec.value(), ec.message());
            m_parent_action->dismay(ec);
        } else {
            COCAINE_LOG_DEBUG(m_parent_action->log(), "request[%d]<dispatch> discarded", m_parent_action->id());
        }
    }

    void
    on_value(const dynamic_t& result){
        if (m_disowned){
            COCAINE_LOG_DEBUG(m_parent_action->log(), "request[%d]<dispatch:disowned> completed", m_parent_action->id());
            return;
        }
        COCAINE_LOG_INFO(m_parent_action->log(), "request[%d]<dispatch> completed", m_parent_action->id());
        m_parent_action->done(result);
    }

    void
    on_error(const std::error_code& ec, const std::string& message){
        auto ec1 = const_cast<std::error_code&>(ec);
        if (m_disowned){
            COCAINE_LOG_DEBUG(m_parent_action->log(), "request[%d]<dispatch:disowned> completed with error [%d] %s", ec.value(), ec.message());
            return;
        }
        COCAINE_LOG_DEBUG(m_parent_action->log(), "request[%d]<dispatch> completed with error [%d] %s", m_parent_action->id(), ec.value(), ec.message());
        m_parent_action->error(ec1);
    }
    
};


} // namespace action


}}} // namespace cocaine::isolate::conductor

#endif
