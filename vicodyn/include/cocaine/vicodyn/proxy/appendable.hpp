#pragma once

#include <cocaine/forwards.hpp>
#include "cocaine/vicodyn/forwards.hpp"

namespace cocaine {
namespace vicodyn {
namespace proxy {

struct appendable_t {
    virtual ~appendable_t() {}
    virtual auto append(const msgpack::object& message, uint64_t event_id, hpack::header_storage_t headers) -> void = 0;
};

typedef std::shared_ptr<appendable_t> appendable_ptr;

} // namespace proxy
} // namespace vicodyn
} // namespace cocaine
