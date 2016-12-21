#pragma once

#include <cocaine/forwards.hpp>

#include "cocaine/vicodyn/proxy/appendable.hpp"

namespace cocaine {
namespace vicodyn {
namespace stream {

class wrapper_t: public proxy::appendable_t {
public:
    wrapper_t(io::upstream_ptr_t upstream);

    auto append(const msgpack::object& message, uint64_t event_id, hpack::header_storage_t headers) -> void override;

private:
    io::upstream_ptr_t upstream;
};

} // namespace stream
} // namespace vicodyn
} // namespace cocaine
