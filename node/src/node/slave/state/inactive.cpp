#include "cocaine/detail/service/node/slave/state/inactive.hpp"

#include "cocaine/service/node/slave/error.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {
namespace state {

inactive_t::inactive_t(std::error_code ec) : ec(std::move(ec)) {}

auto inactive_t::name() const noexcept -> const char* {
    switch (ec.value()) {
    case 0:
    case error::committed_suicide:
        return "closed";
    default:
        return "broken";
    }
}

}  // namespace state
}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
