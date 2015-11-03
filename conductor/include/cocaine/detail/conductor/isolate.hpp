
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
    std::shared_ptr<container_t> m_container;

    handle_t(std::shared_ptr<container_t> container);

    virtual
    void
    terminate();

    virtual
    int
    stdout() const;

};


class isolate_t:
    public api::isolate_t,
    public std::enable_shared_from_this<isolate_t>
{

    std::string          m_name;
    dynamic_t            m_profile;

    std::shared_ptr<cocaine::logging::log_t> m_log;

    std::shared_ptr<client_t> m_client;

    void
    post_handler(std::function<void()> handler);

    std::unique_ptr<api::cancellation_t>
    post_action(std::shared_ptr<action::action_t> action);
    
public:

    isolate_t(context_t& context,
              asio::io_service& io_context,
              const std::string& name,
              const dynamic_t& args);

    virtual
    void
    spool (){
        // Empty
    }

    virtual
    std::unique_ptr<api::handle_t>
    spawn(const std::string& path, const api::string_map_t& args, const api::string_map_t& environment){
        // Fake and empty. Never called.
        return std::move(std::unique_ptr<api::handle_t>());
    }


    virtual
    std::unique_ptr<api::cancellation_t>
    async_spool(std::function<void(const std::error_code&)> handler);
    

    virtual
    std::unique_ptr<api::cancellation_t>
    async_spawn(const std::string& path,
                const api::string_map_t& args,
                const api::string_map_t& environment,
                api::spawn_handler_t handler);

    virtual
    ~isolate_t();

};


}}}


#endif

