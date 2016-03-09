#include "cocaine/detail/service/node/slave/state/terminate.hpp"

#include <asio/deadline_timer.hpp>

#include <blackhole/logger.hpp>

#include "cocaine/api/isolate.hpp"

#include "cocaine/detail/service/node/slave/control.hpp"
#include "cocaine/detail/service/node/slave/machine.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {
namespace state {

namespace ph = std::placeholders;

terminate_t::terminate_t(std::shared_ptr<machine_t> slave_, std::unique_ptr<api::handle_t> handle_,
                         std::shared_ptr<control_t> control_, std::shared_ptr<session_t> session_)
    : slave(std::move(slave_)),
      handle(std::move(handle_)),
      control(std::move(control_)),
      session(std::move(session_)),
      timer(slave->loop) {}

terminate_t::~terminate_t() {
    control->cancel();
    session->detach(asio::error::operation_aborted);

    COCAINE_LOG_DEBUG(slave->log, "state '{}' has been destroyed", name());
}

auto terminate_t::terminating() const noexcept -> bool {
    return true;
}

auto terminate_t::name() const noexcept -> const char* {
    return "terminating";
}

auto terminate_t::cancel() -> void {
    COCAINE_LOG_DEBUG(slave->log, "processing termination timer cancellation");

    try {
        const auto cancelled = timer.cancel();
        COCAINE_LOG_DEBUG(slave->log,
                          "processing termination timer cancellation: done ({} cancelled)",
                          cancelled);
    } catch (const std::system_error& err) {
        COCAINE_LOG_WARNING(slave->log, "unable to cancel termination timer: {}", err.what());
    }
}

auto terminate_t::terminate(const std::error_code& ec) -> void {
    cancel();
    slave->shutdown(ec);
}

auto terminate_t::start(unsigned long timeout, const std::error_code& ec) -> void {
    COCAINE_LOG_DEBUG(slave->log, "slave is terminating, timeout: {} ms", timeout);

    timer.expires_from_now(boost::posix_time::milliseconds(static_cast<std::int64_t>(timeout)));
    timer.async_wait(std::bind(&terminate_t::on_timeout, shared_from_this(), ph::_1));

    // The following operation may fail if the session is already disconnected. In this case a slave
    // shutdown operation will be triggered, which immediately stops the timer.
    control->terminate(ec);
}

auto terminate_t::on_timeout(const std::error_code& ec) -> void {
    if (ec) {
        COCAINE_LOG_DEBUG(slave->log,
                          "termination timer has called its completion handler: cancelled");
    } else {
        COCAINE_LOG_ERROR(slave->log, "unable to terminate slave: timeout");

        slave->shutdown(error::teminate_timeout);
    }
}

}  // namespace state
}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
