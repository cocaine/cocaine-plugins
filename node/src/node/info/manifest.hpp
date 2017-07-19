#pragma once

#include "mod.hpp"

#include "cocaine/idl/node.hpp"
#include "cocaine/service/node/manifest.hpp"

namespace cocaine {
namespace service {
namespace node {
namespace info {

class manifest_t : public collector_t {
    cocaine::manifest_t manifest;
    cocaine::io::node::info::flags_t flags;

public:
    manifest_t(cocaine::manifest_t manifest, cocaine::io::node::info::flags_t flags);

    auto
    apply(dynamic_t::object_t& result) -> void override;

private:
    auto
    should_expand() const -> bool;
};

} // namespace info
} // namespace node
} // namespace service
} // namespace cocaine
