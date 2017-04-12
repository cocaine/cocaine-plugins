#include "cocaine/service/node/overseer.hpp"

#include <blackhole/logger.hpp>

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

#include "cocaine/service/node/slave/id.hpp"

#include "cocaine/detail/service/node/slave.hpp"

#include "engine.hpp"
#include "pool_observer.hpp"

namespace cocaine {
namespace service {
namespace node {

overseer_t::overseer_t(context_t& context,
                       manifest_t manifest,
                       profile_t profile,
                       std::shared_ptr<pool_observer> observer,
                       std::shared_ptr<asio::io_service> loop)
    : engine(std::make_shared<engine_t>(context, manifest, profile, observer, loop)) {}

overseer_t::~overseer_t() {
    COCAINE_LOG_DEBUG(engine->log, "overseer is processing terminate request");

    engine->stopped = true;
    engine->control_population(0);
    engine->pool->clear();
    engine->on_spawn_rate_timer->reset();
}

auto overseer_t::active_workers() const -> std::uint32_t {
    return engine->active_workers();
}

auto overseer_t::manifest() const -> manifest_t {
    return engine->manifest();
}

auto overseer_t::profile() const -> profile_t {
    return engine->profile();
}

auto overseer_t::info(io::node::info::flags_t flags) const -> dynamic_t::object_t {
    return engine->info(flags);
}

auto overseer_t::uptime() const -> std::chrono::seconds {
    return engine->uptime();
}

auto overseer_t::control_population(int count) -> void {
    return engine->control_population(count);
}

auto overseer_t::enqueue(app::event_t event, upstream<io::stream_of<std::string>::tag> downstream)
    -> std::shared_ptr<client_rpc_dispatch_t>
{
    return engine->enqueue(std::move(event), std::move(downstream));
}

auto overseer_t::enqueue(app::event_t event, std::shared_ptr<api::stream_t> rx)
    -> std::shared_ptr<api::stream_t>
{
    return engine->enqueue(std::move(event), std::move(rx));
}

auto overseer_t::prototype() -> io::dispatch_ptr_t {
    return engine->prototype();
}

}  // namespace node
}  // namespace service
}  // namespace cocaine
