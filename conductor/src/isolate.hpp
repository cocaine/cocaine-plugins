
#ifndef COCAINE_CONDUCTOR_ISOLATE_HPP
#define COCAINE_CONDUCTOR_ISOLATE_HPP

#include "common.hpp"
#include "client.hpp"

#include "client_state.hpp"
#include "actions.hpp"



namespace cocaine { namespace isolate { namespace conductor {

struct handle_t:
    public api::handle_t
{
    shared_ptr<container_t> m_container;

    handle_t(shared_ptr<container_t> container);

    virtual
    void
    terminate();

    virtual
    int
    stdout() const;

};


class isolate_t:
    public api::isolate_t,
    public enable_shared_from_this<isolate_t>
{

    std::string          m_name;
    dynamic_t            m_profile;

    std::shared_ptr<cocaine::logging::log_t> m_log;

    shared_ptr<client_t> m_client;

    void
    post_handler(std::function<void()> handler);

    void
    post_action(shared_ptr<action::action_t> action);
    
public:

    isolate_t(context_t& context,
              asio::io_service& io_context,
              const std::string& name,
              const dynamic_t& args);


    virtual
    unique_ptr<api::cancellation_t>
    async_spool(std::function<void(const std::error_code&)> handler);
    

    virtual
    //unique_ptr<api::cancellation_t>
    void
    async_spawn(const std::string& path,
                const api::string_map_t& args,
                const api::string_map_t& environment,
                api::spawn_handler_t handler);

};


}}}


#endif

