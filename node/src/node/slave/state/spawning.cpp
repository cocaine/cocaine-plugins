#include "cocaine/detail/service/node/slave/state/spawning.hpp"

#include <cocaine/rpc/actor.hpp>

#include "cocaine/detail/service/node/slave.hpp"
#include "cocaine/detail/service/node/slave/state/handshaking.hpp"
#include "cocaine/detail/service/node/util.hpp"

namespace ph = std::placeholders;

using namespace cocaine;

spawning_t::spawning_t(std::shared_ptr<state_machine_t> slave_) :
    slave(std::move(slave_)),
    timer(slave->loop)
{}

spawning_t::~spawning_t() {
    COCAINE_LOG_DEBUG(slave->log, "spawning_t is being destroyed");
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

    // Fill Locator's endpoint list.
    std::ostringstream stream;
    auto it = endpoints.begin();
    stream << *it;
    ++it;

    for (; it != endpoints.end(); ++it) {
        stream << "," << *it;
    }

    // Prepare command line arguments for worker instance.
    COCAINE_LOG_DEBUG(slave->log, "preparing command line arguments");
    std::map<std::string, std::string> args;
    args["--uuid"]     = slave->context.id;
    args["--app"]      = slave->context.manifest.name;
    args["--endpoint"] = slave->context.manifest.endpoint;
    args["--locator"]  = stream.str();
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

        //auto on_spawn_handler = trace_t::bind(&spawning_t::on_spawn, shared_from_this(), std::chrono::high_resolution_clock::now());
        auto on_spawn_handler = std::bind(&spawning_t::on_spawn, shared_from_this(), std::chrono::high_resolution_clock::now());

        //int i = 0;

        isolate->async_spawn(
            slave->context.manifest.executable,
            args,
            slave->context.manifest.environment,
            [=] (const std::error_code& ec, std::unique_ptr<api::handle_t>& handle_){

                COCAINE_LOG_DEBUG(slave->log, "async spawn callback here");

                //[&] (const std::error_code& ec){

                // do something with ec
                if (ec){
                    //i++;
                    //throw new std::system_error::system_error(ec.value(), "spawn failed");
                }

                isolate->fake(); // it's empty

                handle = std::move(handle_);
                slave->loop.post(on_spawn_handler);
            }

            // [this, &i, self, isolate, on_spawn_handler] (const std::error_code& ec, std::unique_ptr<api::handle_t>& handle_){

            //     COCAINE_LOG_DEBUG(slave->log, "async spawn callback here");

            //     //[&] (const std::error_code& ec){

            //     // do something with ec
            //     if (ec){
            //         i++;
            //         //throw new std::system_error::system_error(ec.value(), "spawn failed");
            //     }

            //     isolate->spool(); // it's empty

            //     handle = std::move(handle_);
            //     slave->loop.post(on_spawn_handler);
            // }
        );

        // slave->loop.post(on_spawn_handler);

        // [&] (std::unique_ptr<api::handle_t> handle_, const std::error_code& ec){

        //     // do something with ec

        //     handle = std::move(handle_);
        //     slave->loop.post(on_spawn_handler);
        // }


        // handle = isolate->spawn(
        //     slave->context.manifest.executable,
        //     args,
        //     slave->context.manifest.environment
        // );

        // Currently we spawn all slaves synchronously, but here is the right place to provide
        // a callback function to the Isolate.
        // NOTE: The callback must be called from the event loop thread, otherwise the behavior
        // is undefined.
        // slave->loop.post(trace_t::bind(
        //     &spawning_t::on_spawn, shared_from_this(), std::chrono::high_resolution_clock::now()
        // ));

    } catch(const std::system_error& err) {
        COCAINE_LOG_ERROR(slave->log, "unable to spawn slave: %s", err.code().message());

        slave->loop.post([=]() {
            slave->shutdown(err.code());
        });
    }
    
    COCAINE_LOG_DEBUG (slave->log, "async spawn call end");

}

void
spawning_t::on_spawn(std::chrono::high_resolution_clock::time_point start) {
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
