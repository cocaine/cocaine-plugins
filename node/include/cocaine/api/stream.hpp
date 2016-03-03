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
    write(const std::string& chunk, hpack::header_storage_t headers) = 0;

    virtual
    void
    error(const std::error_code& ec, const std::string& reason, hpack::header_storage_t headers) = 0;

    virtual
    void
    close(hpack::header_storage_t headers) = 0;

    // This should be only called once.
    virtual
    hpack::header_storage_t
    initial_headers() {
        return {};
    }
};

}  // namespace api
}  // namespace cocaine
