#pragma once

#include <asio/deadline_timer.hpp>

#include "state.hpp"

#include "cocaine/detail/service/node/forwards.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {
namespace state {

class seal_t:
    public state_t,
    public std::enable_shared_from_this<seal_t>
{
    std::shared_ptr<machine_t> slave;
    std::unique_ptr<api::handle_t> handle;
    std::shared_ptr<control_t> control;
    std::shared_ptr<session_t> session;

    asio::deadline_timer timer;

public:
    seal_t(std::shared_ptr<machine_t> slave,
              std::unique_ptr<api::handle_t> handle,
              std::shared_ptr<control_t> control,
              std::shared_ptr<session_t> session);

    virtual
    void
    cancel();

    virtual
    const char*
    name() const noexcept {
        return "sealing";
    }

    virtual
    bool
    sealing() const noexcept {
        return true;
    }

    void
    start(unsigned long timeout);

    virtual
    void
    terminate(const std::error_code& ec);

private:
    void
    on_timeout(const std::error_code& ec);
};

}  // namespace state
}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
