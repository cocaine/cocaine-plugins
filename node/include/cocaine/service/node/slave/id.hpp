#pragma once

#include <string>

namespace cocaine {
namespace service {
namespace node {
namespace slave {

/// Represents a slave id.
class id_t {
    std::string data;

public:
    /// Constructs a slave id using generated UUID.
    id_t();

    /// Constructs a slave id from the given value.
    id_t(std::string id);

    /// Returns a const lvalue reference to the underlying value.
    auto id() const noexcept -> const std::string&;
};

}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace cocaine
