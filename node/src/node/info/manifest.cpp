#include "manifest.hpp"

namespace cocaine {
namespace service {
namespace node {
namespace info {

manifest_t::manifest_t(cocaine::manifest_t manifest, cocaine::io::node::info::flags_t flags) :
    manifest(manifest),
    flags(flags)
{}

auto
manifest_t::apply(dynamic_t::object_t& result) -> void {
    if (should_expand()) {
        result["manifest"] = manifest.object();
    }
}

auto
manifest_t::should_expand() const -> bool {
    return flags & cocaine::io::node::info::expand_manifest;
}

} // namespace info
} // namespace node
} // namespace service
} // namespace cocaine
