#include <cocaine/rpc/upstream.hpp>

#include "cocaine/service/node/slave/error.hpp"

#include "cocaine/detail/service/node/slave/state/state.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {
namespace state {

state_t::~state_t() = default;

auto state_t::cancel() -> void {}

auto state_t::active() const noexcept -> bool {
    return false;
}

auto state_t::sealing() const noexcept -> bool {
    return false;
}

auto state_t::terminating() const noexcept -> bool {
    return false;
}

auto state_t::activate(std::shared_ptr<session_t>, upstream<io::worker::control_tag>)
    -> std::shared_ptr<control_t> {
    throw_invalid_state();
}

auto state_t::inject(std::shared_ptr<const dispatch<io::stream_of<std::string>::tag>>)
    -> io::upstream_ptr_t {
    throw_invalid_state();
}

auto state_t::seal() -> void {
    throw_invalid_state();
}

auto state_t::terminate(const std::error_code & /*ec*/) -> void {}

auto state_t::throw_invalid_state() -> void {
    throw std::system_error(error::invalid_state, format("invalid state - %s", name()));
}

}  // namespace state
}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
