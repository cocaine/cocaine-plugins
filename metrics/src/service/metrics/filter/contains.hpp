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
    create(const factory_t&, const dynamic_t::array_t& args) const ->
        libmetrics::query_t override
    {
        auto name = args[0].as_string();
        auto value = args[1].as_string();
        return [=](const libmetrics::tags_t& tags) -> bool {
            const auto tag = tags.tag(name);
            return tag && tag->find(value) != std::string::npos;
        };
    }
};

}  // namespace filter
}  // namespace metrics
}  // namespace service
}  // namespace cocaine
