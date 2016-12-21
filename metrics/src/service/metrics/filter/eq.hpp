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
    create(const factory_t&, const dynamic_t::array_t& args) const ->
        libmetrics::query_t override
    {
        auto name = args[0].as_string();
        auto value = args[1].as_string();
        return [=](const libmetrics::tags_t& tags) -> bool {
            return tags.tag(name) == boost::optional<const std::string&>(value);
        };
    }
};

}  // namespace filter
}  // namespace metrics
}  // namespace service
}  // namespace cocaine
