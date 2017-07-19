#pragma once

#include <cocaine/dynamic.hpp>

namespace cocaine {
namespace service {
namespace node {
namespace info {

class collector_t {
public:
    virtual ~collector_t() = default;

    virtual auto apply(dynamic_t::object_t& result) -> void = 0;
};

} // namespace info
} // namespace node
} // namespace service
} // namespace cocaine
