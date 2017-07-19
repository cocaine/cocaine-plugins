#pragma once

#include <cocaine/dynamic.hpp>

#include <asio/ip/tcp.hpp>

namespace cocaine {

template<>
struct dynamic_converter<asio::ip::tcp::endpoint> {
    typedef asio::ip::tcp::endpoint result_type;

    static const bool enable = true;

    static inline
    result_type
    convert(const dynamic_t& from) {
        if (!from.is_array() || from.as_array().size() != 2) {
            throw std::runtime_error("invalid dynamic value for endpoint deserialization");
        }
        auto ep_pair = from.as_array();
        if (!ep_pair[0].is_string() || !ep_pair[1].is_uint()) {
            throw std::runtime_error("invalid dynamic value for endpoint deserialization");
        }
        std::string host = ep_pair[0].to<std::string>();
        unsigned short int port = ep_pair[1].to<unsigned short int>();
        asio::ip::tcp::endpoint result(asio::ip::address::from_string(host), port);
        return result;
    }

    static inline
    bool
    convertible(const dynamic_t& from) {
        return from.is_array() &&
               from.as_array().size() == 2 &&
               from.as_array()[0].is_string() &&
               (from.as_array()[1].is_uint() || from.as_array()[1].is_int());
    }
};

template<>
struct dynamic_constructor<asio::ip::tcp::endpoint> {
    static const bool enable = true;

    static inline
    void
    convert(const asio::ip::tcp::endpoint from, dynamic_t::value_t& to) {
        dynamic_constructor<dynamic_t::array_t>::convert(dynamic_t::array_t(), to);
        auto& array = boost::get<detail::dynamic::incomplete_wrapper<dynamic_t::array_t>>(to).get();
        array.resize(2);
        array[0] = from.address().to_string();
        array[1] = from.port();
    }
};

} // namespace cocaine
