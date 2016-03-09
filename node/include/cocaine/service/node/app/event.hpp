#pragma once

#include <chrono>
#include <string>

#include <cocaine/hpack/header.hpp>

namespace cocaine {
namespace service {
namespace node {
namespace app {

class event_t {
public:
    std::string name;
    std::chrono::high_resolution_clock::time_point birthstamp;

    // TODO: Add urgency policy || lookup headers.
    // TODO: Add deadline policy || lookup headers.

    hpack::header_storage_t headers;

    event_t(std::string name, hpack::header_storage_t headers)
        : name(std::move(name)),
          birthstamp(std::chrono::high_resolution_clock::now()),
          headers(std::move(headers)) {}
};

}  // namespace app
}  // namespace node
}  // namespace service
}  // namespace cocaine
