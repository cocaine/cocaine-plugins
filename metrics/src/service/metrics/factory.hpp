#pragma once

#include "cocaine/service/metrics/fwd.hpp"

namespace cocaine {
namespace service {
namespace metrics {

/// An interface for AST node factory.
template<typename R>
class node_factory {
public:
    typedef R result_type;

public:
    virtual ~node_factory() = default;

    /// Returns AST node name to be constructed.
    virtual
    auto
    name() const -> const char* = 0;

    /// Returns AST node children count or `boost::none` if there can be undefined number of
    /// children.
    ///
    /// This value is checked before calling construction method.
    virtual
    auto
    children() const -> boost::optional<std::size_t> = 0;

    /// Constructs new AST node.
    virtual
    auto
    construct(const registry_t& registry, const dynamic_t::array_t& args) const -> node<R> = 0;
};

}  // namespace metrics
}  // namespace service
}  // namespace cocaine
