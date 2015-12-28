#pragma once

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
    write(const std::string& chunk) = 0;

    virtual
    void
    error(const std::error_code& ec, const std::string& reason) = 0;

    virtual
    void
    close() = 0;
};

}  // namespace api
}  // namespace cocaine
