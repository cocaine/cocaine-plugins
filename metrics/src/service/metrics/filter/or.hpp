#include "../filter.hpp"

namespace cocaine {
namespace service {
namespace metrics {
namespace filter {

class or_t : public filter_t {
public:
    auto
    name() const -> const char* override {
        return "or";
    }

    auto
    arity() const -> boost::optional<std::size_t> override {
        return boost::none;
    }

    auto
    create(const registry_t& registry, const dynamic_t::array_t& args) const ->
        libmetrics::query_t override
    {
        std::vector<libmetrics::query_t> functions;
        std::transform(
            std::begin(args),
            std::end(args),
            std::back_inserter(functions),
            [&](const dynamic_t& arg) {
                return registry.make_filter(arg);
            }
        );

        return [=](const libmetrics::tagged_t& metric) -> bool {
            return std::any_of(
                std::begin(functions),
                std::end(functions),
                [&](const libmetrics::query_t& q) -> bool {
                    return q(metric);
                }
            );
        };
    }
};

}  // namespace filter
}  // namespace metrics
}  // namespace service
}  // namespace cocaine
