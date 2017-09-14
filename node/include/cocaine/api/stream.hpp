#pragma once

#include <cocaine/forwards.hpp>

#include <string>
#include <system_error>

namespace cocaine {
namespace api {

class stream_t {
public:
    virtual ~stream_t() = 0;

    virtual auto write(hpack::headers_t headers, const std::string& chunk) -> stream_t& = 0;
    virtual auto error(hpack::headers_t headers, const std::error_code& ec, const std::string& reason) -> void = 0;
    virtual auto close(hpack::headers_t headers) -> void = 0;
};

}  // namespace api
}  // namespace cocaine
