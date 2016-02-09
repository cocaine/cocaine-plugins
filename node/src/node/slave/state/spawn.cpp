#include "cocaine/detail/service/node/slave/state/spawn.hpp"

#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include <blackhole/logger.hpp>

#include <cocaine/rpc/actor.hpp>
#include <cocaine/detail/service/node/slave/spawn_handle.hpp>

#include "cocaine/api/isolate.hpp"
#include "cocaine/service/node/slave/id.hpp"

#include "cocaine/detail/service/node/slave/machine.hpp"
#include "cocaine/detail/service/node/slave/spawn_handle.hpp"
#include "cocaine/detail/service/node/slave/state/handshaking.hpp"
#include "cocaine/detail/service/node/util.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {
namespace state {

namespace ph = std::placeholders;

using asio::ip::tcp;

spawn_t::spawn_t(std::shared_ptr<machine_t> slave_)
    : slave(std::move(slave_)), timer(slave->loop) {}

auto spawn_t::name() const noexcept -> const char* {
    return "spawning";
}

auto spawn_t::cancel() -> void {
    COCAINE_LOG_DEBUG(slave->log, "processing spawn timer cancellation");

    try {
        const auto cancelled = timer.cancel();
        COCAINE_LOG_DEBUG(slave->log, "processing spawn timer cancellation: done ({} cancelled)",
                          cancelled);
    } catch (const std::system_error& err) {
        // If we are here, then something weird occurs with the timer.
        COCAINE_LOG_WARNING(slave->log, "unable to cancel spawn timer: {}", err.what());
    }
}

auto spawn_t::terminate(const std::error_code& ec) -> void {
    cancel();
    slave->shutdown(ec);
}

auto spawn_t::spawn(unsigned long timeout) -> void {
    COCAINE_LOG_DEBUG(slave->log, "slave is spawning using '{}', timeout: {} ms",
                      slave->manifest.executable, timeout);

    COCAINE_LOG_DEBUG(slave->log, "locating the Locator endpoint list");
    const auto locator = slave->context.locate("locator");

    if (!locator) {
        COCAINE_LOG_ERROR(slave->log, "unable to spawn slave: failed to locate the Locator");
        slave->shutdown(error::locator_not_found);
        return;
    }

    const auto endpoints = locator->endpoints();

    if (endpoints.empty()) {
        COCAINE_LOG_ERROR(slave->log,
                          "unable to spawn slave: failed to determine the Locator endpoints");
        slave->shutdown(error::empty_locator_endpoints);
        return;
    }

    // Prepare command line arguments for worker instance.
    COCAINE_LOG_DEBUG(slave->log, "preparing command line arguments");
    std::map<std::string, std::string> args;
    args["--uuid"] = slave->id.id();
    args["--app"] = slave->manifest.name;
    args["--endpoint"] = slave->manifest.endpoint;
    args["--locator"] = boost::join(
        endpoints | boost::adaptors::transformed(boost::lexical_cast<std::string, tcp::endpoint>),
        ",");
    args["--protocol"] = std::to_string(io::protocol<io::worker_tag>::version::value);

    // Spawn a worker instance and start reading standard outputs of it.
    try {
        auto isolate = slave->context.get<api::isolate_t>(
            slave->profile.isolate.type, slave->context, slave->loop, slave->manifest.name,
            slave->profile.isolate.type, slave->profile.isolate.args);

        COCAINE_LOG_DEBUG(slave->log, "spawning");

        timer.expires_from_now(boost::posix_time::milliseconds(static_cast<std::int64_t>(timeout)));
        timer.async_wait(trace_t::bind(&spawn_t::on_timeout, shared_from_this(), ph::_1));

        std::shared_ptr<api::spawn_handle_base_t> spawn_handle = std::make_shared<spawn_handle_t>(slave, shared_from_this());
        handle = isolate->spawn(
            slave->manifest.executable,
            args,
            slave->manifest.environment,
            spawn_handle
        );

    } catch(const std::system_error& err) {
        COCAINE_LOG_ERROR(slave->log, "unable to spawn slave: {}", err.code().message());

        slave->loop.post([=]() { slave->shutdown(err.code()); });
    }
}

void
spawn_t::on_timeout(const std::error_code& ec) {
    if (ec) {
        COCAINE_LOG_DEBUG(slave->log, "spawn timer has called its completion handler: cancelled");
    } else {
        COCAINE_LOG_ERROR(slave->log, "unable to spawn slave: timeout");

        slave->shutdown(error::spawn_timeout);
    }
}

}  // namespace state
}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
