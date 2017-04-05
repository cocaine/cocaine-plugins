#include <memory>
#include <system_error>

#include <cocaine/forwards.hpp>

#include "cocaine/detail/isolate/process/cgroup.hpp"

namespace cocaine {
namespace error {

auto cgroup_category() -> const std::error_category& {
    static cgroup_category_t instance;
    return instance;
}

}  // namespace error

namespace isolate {


void* init_cgroups(const char*, const dynamic_t&, logging::logger_t&) {
    return nullptr;
}

void destroy_cgroups(void*, logging::logger_t&) {
    // Pass.
}

void attach_cgroups(void*, logging::logger_t&) {
    // Pass.
}

const char* get_cgroup_error(int) {
    return "can not get error message, cgroup support was disabled during compilation";
}

} // namespace isolate
} // namespace cocaine
