#pragma once

#include <type_traits>

namespace cocaine { namespace detail {

template<class F>
struct move_wrapper:
    public F
{
    /* implicit */
    move_wrapper(F&& f):
        F(std::move(f))
    {}

    /// The wrapper declares a copy constructor, tricking asio's machinery into submission,
    /// but never defines it, so that copying would result in a linking error.
    move_wrapper(const move_wrapper& other);

    move_wrapper&
    operator=(const move_wrapper& other);

    move_wrapper(move_wrapper&&) = default;

    move_wrapper&
    operator=(move_wrapper&&) = default;
};

template<class T>
auto
move_handler(T&& t) -> move_wrapper<typename std::decay<T>::type> {
    return std::move(t);
}

template<typename T>
inline constexpr
const T&
bound(const T& min, const T& value, const T& max) {
    return std::max(min, std::min(value, max));
}

}} // namespace cocaine::detail
