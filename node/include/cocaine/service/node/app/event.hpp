#pragma once

#include <chrono>
#include <string>

#include <boost/optional/optional.hpp>

#include <cocaine/hpack/header.hpp>

namespace cocaine {
namespace service {
namespace node {
namespace app {

class event_t {
public:
    typedef std::chrono::high_resolution_clock clock_type;

    std::string name;
    clock_type::time_point birthstamp;
    boost::optional<clock_type::time_point> deadline;

    hpack::header_storage_t headers;

    event_t(std::string name, hpack::header_storage_t headers)
        : name(std::move(name)),
          birthstamp(clock_type::now()),
          headers(std::move(headers)) {}
};

}  // namespace app
}  // namespace node
}  // namespace service
}  // namespace cocaine
