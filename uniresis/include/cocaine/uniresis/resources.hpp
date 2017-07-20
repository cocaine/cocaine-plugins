#pragma once

#include <cstdint>

namespace cocaine {
namespace uniresis {

struct resources_t {
    unsigned int cpu;
    std::uint64_t mem;

    resources_t();
};

} // namespace uniresis
} // namespace cocaine
