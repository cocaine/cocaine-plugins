#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include <boost/optional/optional.hpp>

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {

struct stats_t {
    /// Current state name.
    std::string state;

    std::uint64_t tx;
    std::uint64_t rx;
    std::uint64_t load;
    std::uint64_t total;

    boost::optional<std::chrono::high_resolution_clock::time_point> age;

    stats_t();
};

}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
