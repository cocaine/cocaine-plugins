
#include <cerrno>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include "client.hpp"
#include "action.hpp"
#include "actions.hpp"
#include "isolate.hpp"



namespace cocaine { namespace isolate { namespace conductor {

container_t::container_t(shared_ptr<client_t> parent,
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
        m_terminated = true;
        // shared_ptr<action::action_t> action(
        //     new action::terminate_t (m_parent, m_container_id)
        // );
        auto action = make_shared<action::terminate_t>(
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
    COCAINE_LOG_DEBUG(m_parent->m_log, "attaching to container stdout fifo at %s", m_stdout_path);
    auto fd = ::open(m_stdout_path.c_str(), O_RDONLY) == -1;
    if(fd == -1) {
        auto errno_ = errno;

        COCAINE_LOG_ERROR(m_parent->m_log, "unable to open container stdout pipe");

        throw std::system_error(errno_, std::system_category(), "unable to open container's stdout named pipe");
    }
    m_fd = fd;
}

unique_ptr<api::handle_t>
container_t::handle(){
    unique_ptr<api::handle_t> handle_(new handle_t(shared_from_this()));

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

client_t::client_t(cocaine::context_t& context, asio::io_service& loop, cocaine::dynamic_t& args):
    m_loop(loop),
    m_log(context.log("conductor/client")),
    m_args(args)
{
        
}

shared_ptr<client_t>
client_t::create(context_t& context, asio::io_service& loop, dynamic_t& args){
    shared_ptr<client_t> client(new client_t(context, loop, args));
    client->m_state = make_shared<state::closed_t>(client);
    return client;
}

void
client_t::enqueue(shared_ptr<action::action_t> action){
    if (action->m_state == action::action_t::states::st_pending){
        m_state->enqueue(action);
    } else {
        COCAINE_LOG_INFO(m_log, "dropping cancelled/done action[%d] state[%d]", action->id(), action->m_state);
    }
}

void
client_t::post(std::function<void()> handler){
    m_loop.post(handler);
}

}}} // namespace cocaine::isolate::conductor
