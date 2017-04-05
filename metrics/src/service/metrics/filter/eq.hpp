#pragma once

#include "../filter.hpp"

namespace cocaine {
namespace service {
namespace metrics {
namespace filter {

class eq_t : public filter_t {
public:
    auto
    name() const -> const char* override {
        return "eq";
    }

    auto
    arity() const -> boost::optional<std::size_t> override {
        return 2;
    }

    auto
    create(const registry_t& registry, const dynamic_t::array_t& args) const ->
        libmetrics::query_t override
    {
        auto f1 = registry.make_extractor(args[0]);
        auto f2 = registry.make_extractor(args[1]);
        return [=](const libmetrics::tagged_t& metric) -> bool {
            return f1(metric) == f2(metric);
        };
    }
};

}  // namespace filter
}  // namespace metrics
}  // namespace service
}  // namespace cocaine
