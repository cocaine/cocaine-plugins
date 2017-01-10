
#ifndef COCAINE_CONDUCTOR_UTIL_HPP
#define COCAINE_CONDUCTOR_UTIL_HPP



#include <typeinfo>
#include <iostream>
#include <cocaine/common.hpp>
#include <cocaine/idl/locator.hpp>
#include <cocaine/idl/logging.hpp>

#include <cocaine/rpc/result_of.hpp>
#include <cocaine/rpc/protocol.hpp>
#include <cocaine/rpc/dispatch.hpp>

#include <cocaine/traits/endpoint.hpp>
#include <cocaine/traits/graph.hpp>
#include <cocaine/traits/map.hpp>
#include <cocaine/traits/vector.hpp>

#include "cocaine/idl/conductor.hpp"


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


#endif
