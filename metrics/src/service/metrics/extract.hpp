#pragma once

#include <boost/optional/optional.hpp>

#include <cocaine/forwards.hpp>

#include "cocaine/service/metrics/fwd.hpp"

#include "factory.hpp"
#include "registry.hpp"

namespace cocaine {
namespace service {
namespace metrics {

/// A constant literal extractor.
class const_t : public node_factory<dynamic_t> {
    auto
    name() const -> const char* override {
        return "const";
    }

    auto
    children() const -> boost::optional<std::size_t> override {
        return 1ul;
    }

    auto
    construct(const registry_t&, const dynamic_t::array_t& args) const -> node<dynamic_t> override {
        const auto value = args[0];
        return [=](const libmetrics::tagged_t&) -> dynamic_t {
            return value;
        };
    }
};

/// Metric name extractor.
class name_t : public node_factory<dynamic_t> {
    auto
    name() const -> const char* override {
        return "name";
    }

    auto
    children() const -> boost::optional<std::size_t> override {
        return 0ul;
    }

    auto
    construct(const registry_t&, const dynamic_t::array_t&) const -> node<dynamic_t> override {
        return [=](const libmetrics::tagged_t& metric) -> dynamic_t {
            return metric.name();
        };
    }
};

/// Metric type extractor.
class type_t : public node_factory<dynamic_t> {
    auto
    name() const -> const char* override {
        return "type";
    }

    auto
    children() const -> boost::optional<std::size_t> override {
        return 0ul;
    }

    auto
    construct(const registry_t&, const dynamic_t::array_t&) const -> node<dynamic_t> override {
        return [=](const libmetrics::tagged_t& metric) -> dynamic_t {
            return metric.tag("type").get();
        };
    }
};

/// Generic metric tag extractor.
class tag_t : public node_factory<dynamic_t> {
public:
    auto
    name() const -> const char* override {
        return "tag";
    }

    auto
    children() const -> boost::optional<std::size_t> override {
        return 1ul;
    }

    auto
    construct(const registry_t& registry, const dynamic_t::array_t& args) const -> node<dynamic_t> override {
        auto ex = registry.make_extractor(args.back());
        return [=](const libmetrics::tagged_t& metric) -> dynamic_t {
            const auto name = ex(metric).as_string();
            if (auto result = metric.tag(name)) {
                return result.get();
            } else {
                return std::string();
            }
        };
    }
};

}  // namespace metrics
}  // namespace service
}  // namespace cocaine
