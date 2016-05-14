#pragma once

namespace cocaine {
namespace detail {

template<typename T>
constexpr const T& bound(const T& min, const T& value, const T& max) {
    return std::max(min, std::min(value, max));
}

}  // namespace detail
}  // namespace cocaine
