#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include <metrics/fwd.hpp>

#include "filter.hpp"

namespace cocaine {
namespace service {
namespace metrics {

class factory_t {
    std::unordered_map<std::string, std::shared_ptr<filter_t>> filters;

public:
    auto
    add(std::shared_ptr<filter_t> filter) -> void {
        filters[filter->name()] = std::move(filter);
    }

    auto
    construct(const std::string& name, const dynamic_t::array_t& args) const -> libmetrics::query_t {
        if (filters.count(name) == 0) {
            throw cocaine::error_t("unknown filter function \"{}\"", name);
        }

        const auto& filter = filters.at(name);
        if (auto arity = filter->arity()) {
            if (*arity != args.size()) {
                throw cocaine::error_t("expected {} arguments for filter \"{}\", found {}",
                    *arity, name, args.size());
            }
        }

        return filter->create(*this, args);
    }

    auto
    construct_query(const dynamic_t& query) const -> libmetrics::query_t {
        if (!query.is_object()) {
            throw cocaine::error_t("query object must be an object");
        }

        auto in = query.as_object();
        if (in.size() != 1) {
            throw cocaine::error_t("query function must have exactly one name");
        }

        const auto& name = std::begin(in)->first;
        const auto& args = std::begin(in)->second.as_array();

        return construct(name, args);
    }
};

}  // namespace metrics
}  // namespace service
}  // namespace cocaine
