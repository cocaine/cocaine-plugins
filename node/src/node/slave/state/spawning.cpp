#include "cocaine/detail/service/node/slave/state/spawning.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include <cocaine/rpc/actor.hpp>

#include "cocaine/detail/service/node/slave.hpp"
#include "cocaine/detail/service/node/slave/state/handshaking.hpp"
#include "cocaine/detail/service/node/util.hpp"

namespace ph = std::placeholders;

using namespace cocaine;

using asio::ip::tcp;

spawning_t::spawning_t(std::shared_ptr<state_machine_t> slave_) :
    slave(std::move(slave_)),
    timer(slave->loop)
{}

spawning_t::~spawning_t() {
    if(spawner){
        spawner->cancel();
    }
}

const char*
spawning_t::name() const noexcept {
    return "spawning";
}

void
spawning_t::cancel() {
    COCAINE_LOG_DEBUG(slave->log, "processing spawn timer cancellation");

    try {
        const auto cancelled = timer.cancel();
        COCAINE_LOG_DEBUG(slave->log, "processing spawn timer cancellation: done (%d cancelled)", cancelled);
    } catch (const std::system_error& err) {
        // If we are here, then something weird occurs with the timer.
        COCAINE_LOG_WARNING(slave->log, "unable to cancel spawn timer: %s", err.what());
    }
}

void
spawning_t::terminate(const std::error_code& ec) {
    cancel();
    slave->shutdown(ec);
}

void
spawning_t::spawn(unsigned long timeout) {
    using asio::ip::tcp;

    COCAINE_LOG_DEBUG(slave->log, "slave is spawning using '%s', timeout: %.2f ms",
                      slave->context.manifest.executable, timeout);

    COCAINE_LOG_DEBUG(slave->log, "locating the Locator endpoint list");
    const auto locator = slave->context.context.locate("locator");

    if (!locator) {
        COCAINE_LOG_ERROR(slave->log, "unable to spawn slave: failed to locate the Locator");
        slave->shutdown(error::locator_not_found);
        return;
    }

    const auto endpoints = locator->endpoints();

    if (endpoints.empty()) {
        COCAINE_LOG_ERROR(slave->log, "unable to spawn slave: failed to determine the Locator endpoints");
        slave->shutdown(error::empty_locator_endpoints);
        return;
    }

    // Prepare command line arguments for worker instance.
    COCAINE_LOG_DEBUG(slave->log, "preparing command line arguments");
    std::map<std::string, std::string> args;
    args["--uuid"]     = slave->context.id;
    args["--app"]      = slave->context.manifest.name;
    args["--endpoint"] = slave->context.manifest.endpoint;
    args["--locator"]  = boost::join(endpoints |
        boost::adaptors::transformed(boost::lexical_cast<std::string, tcp::endpoint>), ",");
    args["--protocol"] = std::to_string(io::protocol<io::worker_tag>::version::value);

    // Spawn a worker instance and start reading standard outputs of it.
    try {
        auto isolate = slave->context.context.get<api::isolate_t>(
            slave->context.profile.isolate.type,
            slave->context.context,
            slave->loop,
            slave->context.manifest.name,
            slave->context.profile.isolate.args
        );

        COCAINE_LOG_DEBUG(slave->log, "spawning");

        timer.expires_from_now(boost::posix_time::milliseconds(timeout));
        timer.async_wait(trace_t::bind(&spawning_t::on_timeout, shared_from_this(), ph::_1));

        auto self = shared_from_this();

        auto start = std::chrono::high_resolution_clock::now();

        spawner = isolate->async_spawn(
            slave->context.manifest.executable,
            args,
            slave->context.manifest.environment,
            [self, isolate, start, this] (const std::error_code& ec, std::unique_ptr<api::handle_t>& handle_){
                handle = std::move(handle_);
                slave->loop.post(trace_t::bind(&spawning_t::on_spawn, self, ec, start));
            }

        );

    } catch(const std::system_error& err) {
        COCAINE_LOG_ERROR(slave->log, "unable to spawn slave: %s", err.code().message());

        slave->loop.post([=]() {
            slave->shutdown(err.code());
        });
    }
}

void
spawning_t::on_spawn(const std::error_code& spawn_ec, std::chrono::high_resolution_clock::time_point start) {
    if (spawn_ec) {
        COCAINE_LOG_ERROR(slave->log, "error spawning slave: %d %s", spawn_ec.value(), spawn_ec.message());
        return;
    }
    std::error_code ec;
    const size_t cancelled = timer.cancel(ec);
    if (ec || cancelled == 0) {
        // If we are here, then the spawn timer has been triggered and the slave has been
        // shutdowned with a spawn timeout error.
        COCAINE_LOG_WARNING(slave->log, "slave has been spawned, but the timeout has already expired");
        return;
    }

    const auto now = std::chrono::high_resolution_clock::now();
    COCAINE_LOG_DEBUG(slave->log, "slave has been spawned in %.2f ms",
        std::chrono::duration<float, std::chrono::milliseconds::period>(now - start).count());

    try {
        // May throw system error when failed to assign native descriptor to the fetcher.

        COCAINE_LOG_DEBUG(slave->log, "slave stdout fd is %d", handle->stdout());

        COCAINE_LOG_DEBUG(slave->log, "make handshaking state...");
        
        auto handshaking = std::make_shared<handshaking_t>(slave, std::move(handle));
        slave->migrate(handshaking);

        COCAINE_LOG_DEBUG(slave->log, "migrated to handshaking state...");

        handshaking->start(slave->context.profile.timeout.handshake);

        COCAINE_LOG_DEBUG(slave->log, "started to handshaking state...");
    } catch (const std::exception& err) {
        COCAINE_LOG_ERROR(slave->log, "unable to activate slave: %s", err.what());

        slave->shutdown(error::unknown_activate_error);
    }
}

void
spawning_t::on_timeout(const std::error_code& ec) {
    if (ec) {
        COCAINE_LOG_DEBUG(slave->log, "spawn timer has called its completion handler: cancelled");
    } else {
        COCAINE_LOG_ERROR(slave->log, "unable to spawn slave: timeout");

        slave->shutdown(error::spawn_timeout);
    }
}
