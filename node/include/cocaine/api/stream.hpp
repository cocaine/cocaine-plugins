#pragma once

#include <cocaine/hpack/header.hpp>

#include <cstdint>
#include <string>
#include <system_error>

namespace cocaine {
namespace api {

class stream_t {
public:
    virtual
    ~stream_t() = 0;

    virtual
    stream_t&
    write(hpack::header_storage_t headers, const std::string& chunk) = 0;

    virtual
    void
    error(hpack::header_storage_t headers, const std::error_code& ec, const std::string& reason) = 0;

    virtual
    void
    close(hpack::header_storage_t headers) = 0;
};

}  // namespace api
}  // namespace cocaine
