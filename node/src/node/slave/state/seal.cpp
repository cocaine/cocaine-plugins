#include "cocaine/detail/service/node/slave/state/seal.hpp"

#include <blackhole/logger.hpp>

#include "cocaine/api/isolate.hpp"

#include "cocaine/detail/service/node/slave/machine.hpp"
#include "cocaine/detail/service/node/slave/state/terminate.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {
namespace state {

namespace ph = std::placeholders;

seal_t::seal_t(std::shared_ptr<machine_t> slave_, std::unique_ptr<api::handle_t> handle_,
               std::shared_ptr<control_t> control_, std::shared_ptr<session_t> session_)
    : slave(std::move(slave_)),
      handle(std::move(handle_)),
      control(std::move(control_)),
      session(std::move(session_)),
      timer(slave->loop) {}

auto seal_t::cancel() -> void {
    COCAINE_LOG_DEBUG(slave->log, "processing seal timer cancellation");

    try {
        const auto cancelled = timer.cancel();
        COCAINE_LOG_DEBUG(slave->log, "processing seal timer cancellation: done ({} cancelled)",
                          cancelled);
    } catch (const std::system_error& err) {
        COCAINE_LOG_WARNING(slave->log, "unable to cancel seal timer: {}", err.what());
    }
}

auto seal_t::start(unsigned long timeout) -> void {
    if (slave->data.channels->empty()) {
        terminate(error::slave_is_sealing);
        return;
    }

    COCAINE_LOG_DEBUG(slave->log, "slave is sealing, timeout: {} ms", timeout);

    timer.expires_from_now(boost::posix_time::milliseconds(static_cast<std::int64_t>(timeout)));
    timer.async_wait(std::bind(&seal_t::on_timeout, shared_from_this(), ph::_1));
}

auto seal_t::terminate(const std::error_code& ec) -> void {
    COCAINE_LOG_DEBUG(slave->log, "slave is terminating after been sealed: {}", ec.message());

    cancel();

    auto terminating = std::make_shared<terminate_t>(slave, std::move(handle), std::move(control),
                                                       std::move(session));

    slave->migrate(terminating);

    terminating->start(slave->profile.timeout.terminate, ec);
}

auto seal_t::on_timeout(const std::error_code& ec) -> void {
    if (ec) {
        COCAINE_LOG_DEBUG(slave->log, "seal timer has called its completion handler: cancelled");
    } else {
        COCAINE_LOG_ERROR(slave->log, "unable to seal slave: timeout");

        terminate(error::seal_timeout);
    }
}

}  // namespace state
}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
