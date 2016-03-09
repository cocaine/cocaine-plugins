#include "cocaine/detail/service/node/slave/state/active.hpp"

#include "cocaine/api/isolate.hpp"

#include "cocaine/detail/service/node/slave/control.hpp"
#include "cocaine/detail/service/node/slave/machine.hpp"
#include "cocaine/detail/service/node/slave/state/seal.hpp"
#include "cocaine/detail/service/node/slave/state/terminate.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {
namespace state {

active_t::active_t(std::shared_ptr<machine_t> slave_, std::unique_ptr<api::handle_t> handle_,
                   std::shared_ptr<session_t> session_, std::shared_ptr<control_t> control_)
    : slave(std::move(slave_)),
      handle(std::move(handle_)),
      session(std::move(session_)),
      control(std::move(control_)) {
    control->start();
}

active_t::~active_t() {
    if (control) {
        control->cancel();
    }

    if (session) {
        session->detach(asio::error::operation_aborted);
    }
}

auto active_t::name() const noexcept -> const char* {
    return "active";
}

auto active_t::active() const noexcept -> bool {
    return true;
}

auto active_t::inject(std::shared_ptr<const dispatch<io::stream_of<std::string>::tag>> dispatch)
    -> io::upstream_ptr_t {
    return session->fork(dispatch);
}

auto active_t::seal() -> void {
    auto sealing =
        std::make_shared<seal_t>(slave, std::move(handle), std::move(control), std::move(session));

    slave->migrate(sealing);

    sealing->start(slave->profile.timeout.seal);
}

auto active_t::terminate(const std::error_code& ec) -> void {
    auto terminating = std::make_shared<terminate_t>(slave, std::move(handle), std::move(control),
                                                     std::move(session));

    slave->migrate(terminating);

    terminating->start(slave->profile.timeout.terminate, ec);
}

}  // namespace state
}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
