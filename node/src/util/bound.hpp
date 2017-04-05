#pragma once

namespace cocaine {
namespace util {

template<typename T>
constexpr
const T&
bound(const T& min, const T& value, const T& max) {
    return std::max(min, std::min(value, max));
}

}  // namespace util
}  // namespace cocaine
