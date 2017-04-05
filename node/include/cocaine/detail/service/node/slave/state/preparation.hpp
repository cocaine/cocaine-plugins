#pragma once

#include <chrono>
#include <memory>
#include <system_error>

#include <cocaine/api/authentication.hpp>

#include "cocaine/detail/service/node/forwards.hpp"
#include "state.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {
namespace state {

class preparation_t :
    public state_t,
    public std::enable_shared_from_this<preparation_t>
{
    std::shared_ptr<machine_t> slave;

public:
    explicit preparation_t(std::shared_ptr<machine_t> slave);

    auto name() const noexcept -> const char* override;
    auto terminate(const std::error_code& ec) -> void override;

    auto start(std::chrono::milliseconds timeout) -> void;

private:
    auto on_refresh(api::authentication_t::token_t token, const std::error_code& ec) -> void;
};

}  // namespace state
}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
