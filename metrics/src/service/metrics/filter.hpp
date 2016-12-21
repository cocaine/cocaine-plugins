#pragma once

#include <boost/optional/optional.hpp>

#include <cocaine/forwards.hpp>

#include "cocaine/service/metrics/fwd.hpp"

namespace cocaine {
namespace service {
namespace metrics {

class filter_t {
public:
    virtual ~filter_t() = default;

    virtual auto
    name() const -> const char* = 0;

    virtual auto
    arity() const -> boost::optional<std::size_t> = 0;

    virtual auto
    create(const factory_t& factory, const dynamic_t::array_t& args) const -> libmetrics::query_t = 0;
};

}  // namespace metrics
}  // namespace service
}  // namespace cocaine
