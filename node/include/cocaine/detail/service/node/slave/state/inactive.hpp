#pragma once

#include "state.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {
namespace state {

class inactive_t : public state_t {
    std::error_code ec;

public:
    explicit inactive_t(std::error_code ec);

    auto name() const noexcept -> const char*;
};

}  // namespace state
}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
