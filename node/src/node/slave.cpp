#include "cocaine/detail/service/node/slave.hpp"

#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/min_element.hpp>

#include <asio/io_service.hpp>

#include <blackhole/logger.hpp>

#include <cocaine/context.hpp>
#include <cocaine/rpc/actor.hpp>
#include <cocaine/rpc/upstream.hpp>

#include "cocaine/api/auth.hpp"
#include "cocaine/api/isolate.hpp"

#include "cocaine/service/node/manifest.hpp"
#include "cocaine/service/node/profile.hpp"

#include "cocaine/detail/service/node/slave/machine.hpp"
#include "cocaine/detail/service/node/slave/stats.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {

namespace ph = std::placeholders;

using blackhole::attribute_list;

using detail::service::node::slave::state::spawn_t;
using detail::service::node::slave::state::inactive_t;

using cocaine::detail::service::node::slave::stats_t;
using cocaine::service::node::slave::id_t;

slave_t::slave_t(context_t& context,
                 id_t id,
                 manifest_t manifest,
                 profile_t profile,
                 std::shared_ptr<api::auth_t> auth,
                 asio::io_service& loop,
                 cleanup_handler fn)
    : ec(error::overseer_shutdowning),
      machine(std::make_shared<machine_t>(context, id, manifest, profile, std::move(auth), loop, fn))
{
    machine->start();

    data.id = id.id();
    data.birthstamp = std::chrono::high_resolution_clock::now();
}

slave_t::~slave_t() {
    // This condition is required, because the class can be moved away.
    if (machine) {
        machine->terminate(std::move(ec));
    }
}

const std::string&
slave_t::id() const noexcept {
    return data.id;
}

long long
slave_t::uptime() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now() - data.birthstamp
    ).count();
}

std::uint64_t
slave_t::load() const {
    BOOST_ASSERT(machine);

    return machine->load();
}

auto slave_t::stats() const -> stats_t {
    return machine->stats();
}

bool
slave_t::active() const noexcept {
    BOOST_ASSERT(machine);

    return machine->active();
}

profile_t
slave_t::profile() const {
    return machine->profile;
}

std::shared_ptr<control_t>
slave_t::activate(std::shared_ptr<session_t> session, upstream<io::worker::control_tag> stream) {
    BOOST_ASSERT(machine);

    return machine->activate(std::move(session), std::move(stream));
}

std::uint64_t
slave_t::inject(load_t& load, machine_t::channel_handler handler) {
    return machine->inject(load, handler);
}

void
slave_t::seal() {
    return machine->seal();
}

void
slave_t::terminate(std::error_code ec) {
    this->ec = std::move(ec);
}

}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
