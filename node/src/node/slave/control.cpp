#include "cocaine/detail/service/node/slave/control.hpp"

#include <blackhole/logger.hpp>

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

#include <cocaine/traits/tuple.hpp>

#include "cocaine/detail/service/node/slave/machine.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {

namespace ph = std::placeholders;

control_t::control_t(std::shared_ptr<machine_t> slave_, upstream<io::worker::control_tag> stream_):
    dispatch<io::worker::control_tag>(format("{}/control", slave_->manifest.name)),
    slave(std::move(slave_)),
    stream(std::move(stream_)),
    timer(slave->loop),
    closed(false)
{
    on<io::worker::heartbeat>(std::bind(&control_t::on_heartbeat, this));
    on<io::worker::terminate>(std::bind(&control_t::on_terminate, this, ph::_1, ph::_2));
}

control_t::~control_t() {
    COCAINE_LOG_DEBUG(slave->log, "control channel has been destroyed");
}

auto control_t::start() -> void {
    COCAINE_LOG_DEBUG(slave->log, "heartbeat timer has been started");

    timer.expires_from_now(boost::posix_time::milliseconds(
        static_cast<std::int64_t>(slave->profile.timeout.heartbeat)));
    timer.async_wait(std::bind(&control_t::on_timeout, shared_from_this(), ph::_1));
}

auto control_t::terminate(const std::error_code& ec) -> void {
    BOOST_ASSERT(ec);

    COCAINE_LOG_DEBUG(slave->log, "sending terminate message");

    try {
        stream = stream.send<io::worker::terminate>(ec.value(), ec.message());
    } catch (const std::system_error& err) {
        COCAINE_LOG_WARNING(slave->log, "failed to send terminate message: {}", err.what());
        slave->shutdown(error::conrol_ipc_error);
    }
}

auto control_t::cancel() -> void {
    closed.store(true);

    try {
        timer.cancel();
    } catch (...) {
        // We don't care.
    }
}

auto control_t::discard(const std::error_code& ec) -> void {
    if (ec && !closed) {
        COCAINE_LOG_DEBUG(slave->log, "control channel has been discarded: {}", ec.message());

        // NOTE: It should be called synchronously, because state machine's status should be updated
        // immediately to prevent infinite looping on events rebalancing.
        slave->shutdown(error::conrol_ipc_error);
    }
}

auto control_t::on_heartbeat() -> void {
    COCAINE_LOG_DEBUG(slave->log, "processing heartbeat message");

    if (closed) {
        COCAINE_LOG_DEBUG(slave->log, "heartbeat message has been dropped: control is closed");
    } else {
        COCAINE_LOG_DEBUG(slave->log, "heartbeat timer has been restarted");

        timer.expires_from_now(boost::posix_time::milliseconds(
            static_cast<std::int64_t>(slave->profile.timeout.heartbeat)));
        timer.async_wait(std::bind(&control_t::on_timeout, shared_from_this(), ph::_1));
    }
}

auto control_t::on_terminate(int /*ec*/, const std::string& reason) -> void {
    COCAINE_LOG_DEBUG(slave->log, "processing terminate message: {}", reason);

    // TODO: Check the error code to diverge between normal and abnormal slave shutdown. More will
    // be implemented after error_categories come.
    slave->shutdown(error::committed_suicide);
}

auto control_t::on_timeout(const std::error_code& ec) -> void {
    // No error containing in error code indicates that the slave has failed to send heartbeat
    // message at least once in profile.timeout.heartbeat milliseconds.
    // In this case we should terminate it.
    if (ec) {
        COCAINE_LOG_DEBUG(slave->log, "heartbeat timer has called its completion handler: cancelled");
    } else {
        COCAINE_LOG_ERROR(slave->log, "heartbeat timer has called its completion handler: timeout");
        slave->shutdown(error::heartbeat_timeout);
    }
}

}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
