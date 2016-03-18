#include "cocaine/detail/service/node/slave.hpp"

#include "cocaine/detail/service/node/slave/machine.hpp"
#include "cocaine/detail/service/node/slave/spawn_handle.hpp"
#include "cocaine/detail/service/node/slave/state/handshaking.hpp"
#include "cocaine/detail/service/node/slave/state/spawn.hpp"

#include "cocaine/service/node/slave/error.hpp"

#include <cocaine/logging.hpp>
#include <cocaine/hpack/header.hpp>

#include <blackhole/logger.hpp>

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {

spawn_handle_t::spawn_handle_t(std::unique_ptr<cocaine::logging::logger_t> _log,
                               std::weak_ptr<machine_t> _slave,
                               std::weak_ptr<state::spawn_t> _spawning) :
    log(std::move(_log)),
    slave(std::move(_slave)),
    spawning(std::move(_spawning)),
    start(std::chrono::high_resolution_clock::now())
{}

void
spawn_handle_t::on_terminate(const std::error_code& ec, const std::string& msg) {
    auto _slave = slave.lock();
    if(!_slave) {
        COCAINE_LOG_INFO(log, "isolation has terminated slave, but it's already gone");
        return;
    }
    if(!ec) {
        COCAINE_LOG_INFO(log, "isolation has successfully terminated slave");
    } else {
        COCAINE_LOG_WARNING(_slave->log, "isolation has abnormally terminated slave: [{}]{} - {}", ec.value(), ec.message(), msg);
    }
    // Force slave termination without waiting for heartbeat failure if it was not terminated before.
    _slave->terminate(ec);
}

void
spawn_handle_t::on_ready() {
    auto _spawning = spawning.lock();
    auto _slave = slave.lock();
    if(!_spawning || !_slave) {
        COCAINE_LOG_WARNING(log, "isolation notified about spawn, but slave or state is already gone");
        return;
    }

    //TODO: Use cancel instead of this.
    std::error_code ec;
    const size_t cancelled = _spawning->timer.cancel(ec);
    if (ec || cancelled == 0) {
        // If we are here, then the spawn timer has been triggered and the slave has been
        // shutdowned with a spawn timeout error.
        COCAINE_LOG_WARNING(_slave->log, "slave has been spawned, but the timeout has already expired");
        return;
    }

    const auto now = std::chrono::high_resolution_clock::now();
    COCAINE_LOG_DEBUG(_slave->log, "slave has been spawned in {} ms",
                      std::chrono::duration<float, std::chrono::milliseconds::period>(now - start).count());

    try {
        // May throw system error when failed to assign native descriptor to the fetcher.
        auto handshaking = std::make_shared<state::handshaking_t>(_slave, std::move(_spawning->handle));
        _slave->migrate(handshaking);

        handshaking->start(_slave->profile.timeout.handshake);
    } catch (const std::exception& err) {
        COCAINE_LOG_ERROR(_slave->log, "unable to activate slave: {}", err.what());

        _slave->shutdown(error::unknown_activate_error);
    }
}

void
spawn_handle_t::on_data(const std::string& data) {
    auto _slave = slave.lock();
    if(_slave) {
        _slave->output(std::move(data));
    }
}

} // namespace slave
} // namespace node
} // namespace service
} // namespace detail
} // namespace cocaine
