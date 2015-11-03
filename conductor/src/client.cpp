
#include <cerrno>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include "cocaine/detail/conductor/client.hpp"
#include "cocaine/detail/conductor/action.hpp"
#include "cocaine/detail/conductor/actions.hpp"
#include "cocaine/detail/conductor/isolate.hpp"



namespace cocaine { namespace isolate { namespace conductor {

container_t::container_t(std::shared_ptr<client_t> parent,
                         std::string container_id,
                         std::string name,
                         dynamic_t profile,
                         std::string stdout_path):
    m_terminated(false),
    m_parent(parent),
    m_container_id(container_id),
    m_name(name),
    m_profile(profile),
    m_stdout_path(stdout_path),
    m_fd(-1)
{}
    
void
container_t::terminate(){
    if (!m_terminated){

        COCAINE_LOG_DEBUG(m_parent->m_log, "container[%s]::terminate", m_container_id);

        m_terminated = true;
        auto action = std::make_shared<action::terminate_t>(
            m_parent,
            m_container_id,
            m_name,
            m_profile
        );

        m_parent->enqueue(action);
    }
}

void
container_t::attach (){
    COCAINE_LOG_DEBUG(m_parent->m_log, "container[%s]::attach  stdout fifo at %s", m_container_id, m_stdout_path);
    auto fd = ::open(m_stdout_path.c_str(), O_RDONLY) == -1;
    //auto fd = ::open("/dev/null", O_RDONLY) == -1;
    if(fd == -1) {
        auto errno_ = errno;

        COCAINE_LOG_ERROR(m_parent->m_log, "container[%s]::attach unable to open stdout pipe", m_container_id);

        throw std::system_error(errno_, std::system_category(), "unable to open container's stdout named pipe");
    }
    m_fd = fd;
}

std::unique_ptr<api::handle_t>
container_t::handle(){
    std::unique_ptr<api::handle_t> handle_(new handle_t(shared_from_this()));

    return std::move(handle_);
}

int
container_t::stdout_fd(){
    return m_fd;
}


void
client_t::cancel(uint64_t request_id){
    m_state->cancel(request_id);
}

client_t::client_t(cocaine::context_t& context, asio::io_service& loop, cocaine::dynamic_t args):
    m_loop(loop),
    max_request_id(123),
    m_args(args),
    m_log(context.log("conductor/client"))
{
    COCAINE_LOG_DEBUG(m_log, "client_t::client_t");
}

std::shared_ptr<client_t>
client_t::create(context_t& context, asio::io_service& loop, const dynamic_t& args){
    auto client = std::make_shared<client_t>(context, loop, args);

    COCAINE_LOG_DEBUG(client->m_log, "client_t::state[none]");

    auto state = std::make_shared<state::closed_t>(client);

    client->migrate(nullptr, state);

    COCAINE_LOG_DEBUG(client->m_log, "client_t::state->closed");
    return client;
}

void
client_t::enqueue(std::shared_ptr<action::action_t> action){
    if (action->m_state == st_pending){
        COCAINE_LOG_DEBUG(m_log, "client::enqueue(action[%d])", action->id());
        m_state->enqueue(action);
    } else {
        COCAINE_LOG_DEBUG(m_log, "client::enqueue action[%d] cancelled/done state[%d], dropping", action->id(), action->m_state);
    }
}

void
client_t::post(std::function<void()> handler){
    m_loop.post(handler);
}

void
client_t::migrate(std::shared_ptr<state::base_t> current_state, std::shared_ptr<state::base_t> new_state) {
    COCAINE_LOG_DEBUG(m_log, "client state transition requested: [%s]->[%s]",
                      current_state? current_state->name(): "null",
                      new_state->name());
    if(m_state == current_state){
        COCAINE_LOG_DEBUG(m_log, "client state transition: [%s]->[%s]",
                          current_state ? current_state->name() : "null",
                          new_state->name());
        m_state = new_state;
    } else {
        COCAINE_LOG_WARNING(m_log, "actual [%s] and current [%s] states don't match",
                            m_state ? m_state->name() : "null",
                            current_state ? current_state->name() : "null");
    }
}

client_t::~client_t(){
    COCAINE_LOG_WARNING(m_log, "client_t::~client_t");
}

}}} // namespace cocaine::isolate::conductor
