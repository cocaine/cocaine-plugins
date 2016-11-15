#include "cocaine/detail/service/node/slave/state/spawn.hpp"

#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include <blackhole/logger.hpp>

#include <cocaine/context.hpp>
#include <cocaine/context/quote.hpp>
#include <cocaine/repository.hpp>
#include <cocaine/repository/isolate.hpp>
#include <cocaine/rpc/actor.hpp>
#include <cocaine/trace/trace.hpp>
#include <cocaine/detail/service/node/slave/spawn_handle.hpp>

#include "cocaine/api/isolate.hpp"
#include "cocaine/service/node/slave/id.hpp"

#include "cocaine/detail/service/node/slave/machine.hpp"
#include "cocaine/detail/service/node/slave/control.hpp"
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

spawn_t::spawn_t(std::shared_ptr<machine_t> slave_) :
    slave(std::move(slave_)),
    timer(slave->loop)
{}

spawn_t::~spawn_t() {
    data.apply([&](data_t& data) {
        if (data.control) {
            data.control->cancel();
        }

        if (data.session) {
            data.session->detach(asio::error::operation_aborted);
        }
    });
}

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

auto spawn_t::spawn(api::auth_t::token_t token, unsigned long timeout) -> void {
    COCAINE_LOG_DEBUG(slave->log, "slave is spawning using '{}', timeout: {} ms",
                      slave->manifest.executable, timeout);

    COCAINE_LOG_DEBUG(slave->log, "locating the Locator endpoint list");
    const auto locator_quote = slave->context.locate("locator");

    if (!locator_quote) {
        COCAINE_LOG_ERROR(slave->log, "unable to spawn slave: failed to locate the Locator");
        slave->shutdown(error::locator_not_found);
        return;
    }

    if (locator_quote->endpoints.empty()) {
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
        locator_quote->endpoints | boost::adaptors::transformed(boost::lexical_cast<std::string, tcp::endpoint>),
        ",");
    args["--protocol"] = std::to_string(io::protocol<io::worker_tag>::version::value);

    // Spawn a worker instance and start reading standard outputs of it.
    try {
        auto isolate = slave->context.repository().get<api::isolate_t>(
            slave->profile.isolate.type,
            slave->context,
            slave->loop,
            slave->manifest.name,
            slave->profile.isolate.type,
            slave->profile.isolate.args
        );

        COCAINE_LOG_DEBUG(slave->log, "spawning");

        timer.expires_from_now(boost::posix_time::milliseconds(static_cast<std::int64_t>(timeout)));
        timer.async_wait(trace_t::bind(&spawn_t::on_timeout, shared_from_this(), ph::_1));

        auto spawn_handle = std::make_shared<spawn_handle_t>(
            slave->context.log("spawn_handle"),
            slave,
            shared_from_this()
        );

        auto env = slave->manifest.environment;
        if (!token.type.empty()) {
            env["COCAINE_APP_TOKEN_TYPE"] = token.type;
            env["COCAINE_APP_TOKEN_BODY"] = token.body;
        }

        handle = isolate->spawn(
            slave->manifest.executable,
            args,
            std::move(env),
            std::move(spawn_handle)
        );
    } catch (const std::system_error& err) {
        COCAINE_LOG_ERROR(slave->log, "unable to spawn slave: {}", error::to_string(err));

        slave->loop.post([=]() { slave->shutdown(err.code()); });
    }
}

std::shared_ptr<control_t>
spawn_t::activate(std::shared_ptr<session_t> session, upstream<io::worker::control_tag> stream) {
    // If we are here, then an application has been spawned and started to work before isolate have
    // been notified about it. Just create control dispatch to be able to handle heartbeats and save
    // it for further usage.
    auto control = data.apply([&](data_t& data) -> std::shared_ptr<control_t> {
        if (data.handshaking) {
            return data.handshaking->activate(std::move(session), std::move(stream));
        } else {
            data.session = session;
            data.control = std::make_shared<control_t>(slave, std::move(stream));

            return data.control;
        }
    });

    return control;
}

void
spawn_t::on_spawn(std::chrono::high_resolution_clock::time_point start) {
    const auto now = std::chrono::high_resolution_clock::now();
    const auto elapsed = std::chrono::duration<float, std::chrono::milliseconds::period>(now - start).count();

    std::error_code ec;
    const auto cancelled = timer.cancel(ec);
    if (ec || cancelled == 0) {
        // If we are here, then the spawn timer has been triggered and the slave has been
        // shutdowned with a spawn timeout error.
        COCAINE_LOG_WARNING(slave->log, "slave has been spawned in {} ms, but the timeout has already expired",
                            elapsed);
        return;
    }

    COCAINE_LOG_DEBUG(slave->log, "slave has been spawned in {} ms", elapsed);

    try {
        data.apply([&](data_t& data) {
            data.handshaking = std::make_shared<handshaking_t>(slave, std::move(handle));
            // May throw system error when failed to assign native descriptor to the fetcher.
            slave->migrate(data.handshaking);

            if (data.control) {
                // We've already received handshake frame and can immediately skip handshaking
                // state.
                data.handshaking->activate(std::move(data.session), std::move(data.control));
            } else {
                data.handshaking->start(slave->profile.timeout.handshake);
            }
        });
    } catch (const std::exception& err) {
        COCAINE_LOG_ERROR(slave->log, "unable to activate slave: {}", err.what());

        slave->loop.post([=]() {
            slave->shutdown(error::unknown_activate_error);
        });
    }
}

auto spawn_t::on_timeout(const std::error_code& ec) -> void {
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
