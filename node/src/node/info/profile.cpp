#include "profile.hpp"

namespace cocaine {
namespace service {
namespace node {
namespace info {

profile_t::profile_t(cocaine::profile_t profile, cocaine::io::node::info::flags_t flags) :
    profile(profile),
    flags(flags)
{}

auto
profile_t::apply(dynamic_t::object_t& result) -> void {
    dynamic_t::object_t info;

    // Useful when you want to edit the profile.
    info["name"] = profile.name;
    if (should_expand()) {
        info["data"] = profile.object();
    }

    result["profile"] = info;
}

auto
profile_t::should_expand() const -> bool {
    return flags & cocaine::io::node::info::expand_profile;
}

} // namespace info
} // namespace node
} // namespace service
} // namespace cocaine
