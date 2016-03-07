#pragma once

#include <cocaine/hpack/header.hpp>

#include <chrono>
#include <string>

namespace cocaine { namespace app {

class event_t {
public:
    const std::string name;
    const std::chrono::high_resolution_clock::time_point birthstamp;
    hpack::header_storage_t headers;

    event_t(std::string _name, hpack::header_storage_t _headers):
        name(std::move(_name)),
        birthstamp(std::chrono::high_resolution_clock::now()),
        headers(std::move(_headers))
    {}
};

}} // namespace cocaine::app
