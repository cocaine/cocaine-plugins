
#ifndef COCAINE_CONDUCTOR_CLIENT_HPP
#define COCAINE_CONDUCTOR_CLIENT_HPP

#include "common.hpp"
#include "client_state.hpp"

namespace cocaine { namespace isolate { namespace conductor {

class conductor_category_t:
    public std::error_category
{
    virtual
    auto
    name() const throw() -> const char* {
        return "cocaine.isolate.conductor";
    }

    virtual
    auto
    message(int code) const -> std::string {
        if (code == 0) {
            return "discard by parent";
        } else {
            return "<unknown code>";
        }
    }
};

const std::error_category&
conductor_category();


struct container_t:
    public std::enable_shared_from_this<container_t>
{
    
    bool                 m_terminated;
    std::shared_ptr<client_t> m_parent;
    std::string          m_container_id;
    std::string          m_name;
    dynamic_t            m_profile;
    std::string          m_stdout_path;
    int                  m_fd;

    container_t(std::shared_ptr<client_t> parent,
                std::string container_id,
                std::string name,
                dynamic_t profile,
                std::string stdout_path);

    ~container_t();
    
    void
    terminate();

    void
    attach();

    std::unique_ptr<api::handle_t>
    handle();

    int
    stdout_fd();
};

class client_t:
    public std::enable_shared_from_this<client_t>
{

    friend class state::base_t;
    friend class state::closed_t;
    friend class state::connecting_t;
    friend class state::connected_t;

    friend class action::action_t;

    friend class action::spool_t;
    friend class action::spawn_t;
    friend class action::terminate_t;

    friend struct container_t;

    std::shared_ptr<state::base_t> m_state;

    asio::io_service&     m_loop;
    std::atomic<uint64_t> max_request_id;
    dynamic_t             m_args;

    std::shared_ptr<cocaine::logging::log_t> m_log;

    std::deque<std::shared_ptr<action::action_t>> m_inbox;

    std::map<
        uint64_t,
        std::shared_ptr<action::action_t>
    > m_requests_pending;

    void
    cancel(uint64_t request_id);

public:

    // XXX prohibit direct usage of all other constructors, too
    // instance creation is allowed only via static create()
    client_t(context_t& context, asio::io_service& loop, dynamic_t args);

    static
    std::shared_ptr<client_t>
    create(context_t& context, asio::io_service& loop, const dynamic_t& args);

    void
    enqueue(std::shared_ptr<action::action_t> action);

    void
    post(std::function<void()> handler);

    void
    reset_requests();

    void
    close();

    void
    migrate(std::shared_ptr<state::base_t> current_state, std::shared_ptr<state::base_t> new_state);

    virtual
    ~client_t();

};

}}} // namespace cocaine::isolate::conductor

#endif
