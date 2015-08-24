#include "cocaine/detail/service/node/slave/state/handshaking.hpp"

#include "cocaine/detail/service/node/slave.hpp"
#include "cocaine/detail/service/node/slave/control.hpp"
#include "cocaine/detail/service/node/slave/fetcher.hpp"
#include "cocaine/detail/service/node/slave/state/active.hpp"

namespace ph = std::placeholders;

using namespace cocaine;

handshaking_t::handshaking_t(std::shared_ptr<state_machine_t> slave_, std::unique_ptr<api::handle_t> handle_):
    slave(std::move(slave_)),
    timer(slave->loop),
    handle(std::move(handle_)),
    birthtime(std::chrono::high_resolution_clock::now())
{
    COCAINE_LOG_DEBUG(slave->log, "slave is attaching the standard output handler");

    slave->fetcher.apply([&](std::shared_ptr<fetcher_t>& fetcher) {
        // If there is no fetcher already - it only means, that the slave has been shutted down
        // externally.
        if (fetcher) {
            fetcher->assign(handle->stdout());
        } else {
            throw std::system_error(error::overseer_shutdowning, "slave is shutdowning");
        }
    });
}

const char*
handshaking_t::name() const noexcept {
    return "handshaking";
}

void handshaking_t::cancel() {
    COCAINE_LOG_DEBUG(slave->log, "processing activation timer cancellation");

    try {
        const auto cancelled = timer->cancel();
        COCAINE_LOG_DEBUG(slave->log, "processing activation timer cancellation: done (%d cancelled)", cancelled);
    } catch (const std::system_error& err) {
        COCAINE_LOG_WARNING(slave->log, "unable to cancel activation timer: %s", err.what());
    }
}

void
handshaking_t::terminate(const std::error_code& ec) {
    cancel();
    slave->shutdown(ec);
}

std::shared_ptr<control_t>
handshaking_t::activate(std::shared_ptr<session_t> session, upstream<io::worker::control_tag> stream) {
    std::error_code ec;

    const size_t cancelled = timer->cancel(ec);
    if (ec || cancelled == 0) {
        COCAINE_LOG_WARNING(slave->log, "slave has been activated, but the timeout has already expired");
        return nullptr;
    }

    const auto now = std::chrono::high_resolution_clock::now();
    COCAINE_LOG_DEBUG(slave->log, "slave has been activated in %.2f ms",
        std::chrono::duration<float, std::chrono::milliseconds::period>(now - birthtime).count());

    try {
        auto control = std::make_shared<control_t>(slave, std::move(stream));
        auto active = std::make_shared<active_t>(slave, std::move(handle), std::move(session), control);
        slave->migrate(active);

        return control;
    } catch (const std::exception& err) {
        COCAINE_LOG_ERROR(slave->log, "unable to activate slave: %s", err.what());

        slave->shutdown(error::unknown_activate_error);
    }

    return nullptr;
}

void
handshaking_t::start(unsigned long timeout) {
    COCAINE_LOG_DEBUG(slave->log, "slave is waiting for handshake, timeout: %.2f ms", timeout);

    timer.apply([&](asio::deadline_timer& timer) {
        timer.expires_from_now(boost::posix_time::milliseconds(timeout));
        timer.async_wait(std::bind(&handshaking_t::on_timeout, shared_from_this(), ph::_1));
    });
}

void
handshaking_t::on_timeout(const std::error_code& ec) {
    if (ec) {
        COCAINE_LOG_DEBUG(slave->log, "activation timer has called its completion handler: cancelled");
    } else {
        COCAINE_LOG_ERROR(slave->log, "unable to activate slave: timeout");

        slave->shutdown(error::activate_timeout);
    }
}
