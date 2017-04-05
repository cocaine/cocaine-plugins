#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include <metrics/fwd.hpp>

#include "extract.hpp"
#include "filter.hpp"

namespace cocaine {
namespace service {
namespace metrics {

class registry_t {
    std::unordered_map<std::string, std::shared_ptr<filter_t>> filters;
    std::unordered_map<std::string, std::shared_ptr<node_factory<dynamic_t>>> extractors;

public:
    auto
    add(std::shared_ptr<filter_t> filter) -> void {
        filters[filter->name()] = std::move(filter);
    }

    auto
    add(std::shared_ptr<node_factory<dynamic_t>> extractor) -> void {
        extractors[extractor->name()] = std::move(extractor);
    }

    auto
    make_filter(const dynamic_t& tree) const -> libmetrics::query_t {
        if (!tree.is_object()) {
            throw cocaine::error_t("AST filter node must be an object");
        }

        const auto syntax_tree = tree.as_object();
        if (syntax_tree.size() != 1) {
            throw cocaine::error_t("AST filter node must have exactly one name");
        }

        const auto& name = std::begin(syntax_tree)->first;
        const auto& args = std::begin(syntax_tree)->second.as_array();

        if (filters.count(name) == 0) {
            throw cocaine::error_t("unknown filter '{}'", name);
        }

        const auto& filter = filters.at(name);
        if (auto arity = filter->arity()) {
            if (*arity != args.size()) {
                throw cocaine::error_t("expected {} arguments for filter '{}', found {}",
                    *arity, name, args.size());
            }
        }

        return filter->create(*this, args);
    }

    auto
    make_extractor(const dynamic_t& tree) const -> node<dynamic_t> {
        if (!tree.is_object()) {
            throw cocaine::error_t("AST extractor node must be an object");
        }

        const auto syntax_tree = tree.as_object();
        if (syntax_tree.size() != 1) {
            throw cocaine::error_t("AST extractor node must have exactly one name");
        }

        const auto& name = std::begin(syntax_tree)->first;
        const auto& args = std::begin(syntax_tree)->second.as_array();

        if (extractors.count(name) == 0) {
            throw cocaine::error_t("unknown extractor '{}'", name);
        }

        const auto& ex = extractors.at(name);
        if (auto nchildren = ex->children()) {
            if (nchildren.get() != args.size()) {
                throw cocaine::error_t("expected {} arguments for extractor '{}', found {}",
                    nchildren.get(), name, args.size());
            }
        }

        return ex->construct(*this, args);
    }
};

}  // namespace metrics
}  // namespace service
}  // namespace cocaine
