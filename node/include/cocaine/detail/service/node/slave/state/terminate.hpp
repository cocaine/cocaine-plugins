#pragma once

#include <memory>

#include <asio/deadline_timer.hpp>

#include "state.hpp"

#include "cocaine/detail/service/node/forwards.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {
namespace state {

class terminate_t : public state_t, public std::enable_shared_from_this<terminate_t> {
    std::shared_ptr<machine_t> slave;
    std::unique_ptr<api::handle_t> handle;
    std::shared_ptr<control_t> control;
    std::shared_ptr<session_t> session;

    asio::deadline_timer timer;

public:
    terminate_t(std::shared_ptr<machine_t> slave, std::unique_ptr<api::handle_t> handle,
                  std::shared_ptr<control_t> control, std::shared_ptr<session_t> session);
    ~terminate_t();

    auto terminating() const noexcept -> bool;
    auto name() const noexcept -> const char*;
    auto cancel() -> void;
    auto terminate(const std::error_code& ec) -> void;

    auto start(unsigned long timeout, const std::error_code& ec) -> void;

private:
    auto on_timeout(const std::error_code& ec) -> void;
};

}  // namespace state
}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
