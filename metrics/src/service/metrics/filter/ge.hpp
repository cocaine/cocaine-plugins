#pragma once

#include "../filter.hpp"

namespace cocaine {
namespace service {
namespace metrics {
namespace filter {

class ge_t : public filter_t {
public:
    auto
    name() const -> const char* override {
        return "ge";
    }

    auto
    arity() const -> boost::optional<std::size_t> override {
        return 2;
    }

    auto
    create(const registry_t&, const dynamic_t::array_t&) const ->
        libmetrics::query_t override
    {
        // auto f1 = factory.construct_query(args[0]);
        // auto f2 = factory.construct_query(args[1]);
        return [=](const libmetrics::tagged_t&) -> bool {
            // return f1(metric) >= f2(metric);
            return true;
        };
    }
};

}  // namespace filter
}  // namespace metrics
}  // namespace service
}  // namespace cocaine
