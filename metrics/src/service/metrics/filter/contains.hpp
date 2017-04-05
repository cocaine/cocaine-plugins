#include "../filter.hpp"

namespace cocaine {
namespace service {
namespace metrics {
namespace filter {

class contains_t : public filter_t {
public:
    auto
    name() const -> const char* override {
        return "contains";
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
            auto name = f1(metric).as_string();
            auto substring = f2(metric).as_string();
            return name.find(substring) != std::string::npos;
        };
    }
};

}  // namespace filter
}  // namespace metrics
}  // namespace service
}  // namespace cocaine
