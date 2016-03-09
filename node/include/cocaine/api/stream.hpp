#pragma once

#include <string>
#include <system_error>

namespace cocaine {
namespace hpack {

class header_storage_t;

}  // namespace hpack
}  // namespace cocaine

namespace cocaine {
namespace api {

class stream_t {
public:
    virtual ~stream_t() = 0;

    virtual auto write(hpack::header_storage_t headers, const std::string& chunk) -> stream_t& = 0;
    virtual auto error(hpack::header_storage_t headers, const std::error_code& ec, const std::string& reason) -> void = 0;
    virtual auto close(hpack::header_storage_t headers) -> void = 0;
};

}  // namespace api
}  // namespace cocaine
