#pragma once

namespace cocaine {
namespace stdext {

template<typename T>
constexpr
auto
clamp(const T& value, const T& min, const T& max) -> const T& {
    return std::max(min, std::min(value, max));
}

} // namespace stdext
} // namespace cocaine
