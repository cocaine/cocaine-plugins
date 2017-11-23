#include "cocaine/uniresis/resources.hpp"

#include <unistd.h>

#include <thread>

namespace cocaine {
namespace uniresis {

namespace {

auto
total_system_memory() -> std::uint64_t {
    long pages = ::sysconf(_SC_PHYS_PAGES);
    long page_size = ::sysconf(_SC_PAGE_SIZE);
    return pages * page_size;
}

} // namespace

resources_t::resources_t() :
    cpu(std::thread::hardware_concurrency()),
    mem(total_system_memory())
{}

} // namespace uniresis
} // namespace cocaine
