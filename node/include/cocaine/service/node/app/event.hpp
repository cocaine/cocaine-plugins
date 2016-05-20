#pragma once

#include <chrono>
#include <string>

#include <boost/optional/optional.hpp>
#include <boost/range/algorithm/find_if.hpp>

#include <cocaine/hpack/header.hpp>

namespace cocaine {
namespace service {
namespace node {
namespace app {

///
/// Headers:
/// deadline - time point in UnixTime after which an event in the queue will be treated as
///     expired and dropped with error.
/// request_timeout - duration in milliseconds starting from creating an event after which a channel
///     will be closed with timeout error if it wasn't closed before.
class event_t {
public:
    typedef std::chrono::high_resolution_clock clock_type;

    std::string name;

    /// Time point when an event was created.
    clock_type::time_point birthstamp;
    boost::optional<clock_type::time_point> deadline;

    hpack::header_storage_t headers;

    event_t(std::string name, hpack::header_storage_t headers)
        : name(std::move(name)),
          birthstamp(clock_type::now()),
          headers(std::move(headers)) {}

    template<typename T, std::size_t N>
    auto header(char const (&name)[N]) const -> boost::optional<T> {
        const auto hname = hpack::header::create_data(name);

        auto it = boost::find_if(headers.get_headers(), [&](const hpack::header_t& item) -> bool {
            return item.get_name() == hname;
        });

        if (it == std::end(headers.get_headers())) {
            return boost::none;
        } else {
            try {
                return it->get_value().template convert<T>();
            } catch (const std::system_error& err) {
                return boost::none;
            }
        }
    }
};

}  // namespace app
}  // namespace node
}  // namespace service
}  // namespace cocaine
