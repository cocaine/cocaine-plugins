#include "cocaine/detail/service/node/slave/state/handshaking.hpp"

#include <blackhole/logger.hpp>

#include "cocaine/api/isolate.hpp"

#include "cocaine/detail/service/node/slave/control.hpp"
#include "cocaine/detail/service/node/slave/machine.hpp"
#include "cocaine/detail/service/node/slave/state/active.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {
namespace state {

namespace ph = std::placeholders;

handshaking_t::handshaking_t(std::shared_ptr<machine_t> slave_, std::unique_ptr<api::cancellation_t> handle_)
    : slave(std::move(slave_)),
      timer(slave->loop),
      handle(std::move(handle_)),
      birthtime(std::chrono::high_resolution_clock::now())
{}

auto handshaking_t::name() const noexcept -> const char* {
    return "handshaking";
}

auto handshaking_t::cancel() -> void {
    COCAINE_LOG_DEBUG(slave->log, "processing activation timer cancellation");

    try {
        const auto cancelled = timer->cancel();
        COCAINE_LOG_DEBUG(
            slave->log, "processing activation timer cancellation: done ({} cancelled)", cancelled);
    } catch (const std::system_error& err) {
        COCAINE_LOG_WARNING(slave->log, "unable to cancel activation timer: {}", err.what());
    }
}

auto handshaking_t::terminate(const std::error_code& ec) -> void {
    cancel();
    slave->shutdown(ec);
}

auto handshaking_t::activate(std::shared_ptr<session_t> session,
                           upstream<io::worker::control_tag> stream) -> std::shared_ptr<control_t> {
    std::error_code ec;

    const size_t cancelled = timer->cancel(ec);
    if (ec || cancelled == 0) {
        COCAINE_LOG_WARNING(slave->log,
                            "slave has been activated, but the timeout has already expired");
        return nullptr;
    }

    const auto now = std::chrono::high_resolution_clock::now();
    COCAINE_LOG_DEBUG(
        slave->log, "slave has been activated in {} ms",
        std::chrono::duration<float, std::chrono::milliseconds::period>(now - birthtime).count());

    try {
        auto control = std::make_shared<control_t>(slave, std::move(stream));
        activate(std::move(session), control);

        return control;
    } catch (const std::exception& err) {
        COCAINE_LOG_ERROR(slave->log, "unable to activate slave: {}", err.what());

        slave->shutdown(error::unknown_activate_error);
    }

    return nullptr;
}

void
handshaking_t::activate(std::shared_ptr<session_t> session, std::shared_ptr<control_t> control) {
    auto active = std::make_shared<active_t>(slave, std::move(handle), std::move(session), control);
    slave->migrate(active);
}

void
handshaking_t::start(unsigned long timeout) {
    COCAINE_LOG_DEBUG(slave->log, "slave is waiting for handshake, timeout: {} ms", timeout);

    timer.apply([&](asio::deadline_timer& timer) {
        timer.expires_from_now(boost::posix_time::milliseconds(static_cast<std::int64_t>(timeout)));
        timer.async_wait(std::bind(&handshaking_t::on_timeout, shared_from_this(), ph::_1));
    });
}

auto handshaking_t::on_timeout(const std::error_code& ec) -> void {
    if (ec) {
        COCAINE_LOG_DEBUG(slave->log,
                          "activation timer has called its completion handler: cancelled");
    } else {
        COCAINE_LOG_ERROR(slave->log, "unable to activate slave: timeout");

        slave->shutdown(error::activate_timeout);
    }
}

}  // namespace state
}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
