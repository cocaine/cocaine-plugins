
#include "isolate.hpp"


namespace cocaine { namespace isolate { namespace conductor {


handle_t::handle_t(shared_ptr<container_t> container):
    m_container(container)
{
    // Empty
}

void
handle_t::terminate() {
    m_container->terminate();
}


int
handle_t::stdout() const {
    return m_container->stdout_fd();
}




void
isolate_t::post_handler(std::function<void()> handler) {
    m_loop->post(handler);
}

void
isolate_t::post_action(shared_ptr<action::action_t> action) {
    auto c = action->cancellation();
    m_loop->post(std::bind(&action->run, action));

    return c;
}
    
isolate_t::isolate_t(context_t& context,  asio::io_service& io_context, const std::string& name, const dynamic_t& args):
    api::isolate_t(context, loop, name, args),
    m_name(name),
    m_profile(args),
    m_log(context.log("app/" + name, {{"isolate", "conductor"}})),
    m_client(client_t::create(context, loop, args))
{
        
}

unique_ptr<api::cancellation_t>
isolate_t::async_spool(std::function<void(const std::error_code&)> handler)
{
    auto action = make_shared<spool_action_t>(
        m_client,
        m_name,
        m_profile
    );

    return post_action(action);
}

void
//unique_ptr<api::cancellation_t>
isolate_t::async_spawn(const std::string& path,
                       const api::string_map_t& args,
                       const api::string_map_t& environment,
                       api::spawn_handler_t handler)
{

    auto action = make_shared<spawn_action_t>(
        m_client,
        m_name,
        m_profile,
        path,
        args,
        environment,
        handler
    );

    post_action(action);
}


}}}


#endif

