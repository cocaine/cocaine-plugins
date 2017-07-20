#include "cocaine/service/uniresis.hpp"

#include <unistd.h>

#include <thread>

#include <blackhole/logger.hpp>

#include <cocaine/context.hpp>
#include <cocaine/dynamic.hpp>

#include "cocaine/uniresis/error.hpp"

namespace cocaine {
namespace service {

namespace {

auto
total_system_nemory() -> unsigned long long {
    long pages = ::sysconf(_SC_PHYS_PAGES);
    long page_size = ::sysconf(_SC_PAGE_SIZE);
    return pages * page_size;
}

} // namespace

uniresis_t::uniresis_t(context_t& context, asio::io_service& loop, const std::string& name, const dynamic_t& args) :
    api::service_t(context, loop, name, args),
    dispatch<io::uniresis_tag>(name),
    cpu(std::thread::hardware_concurrency()),
    mem(total_system_nemory()),
    log(context.log("uniresis"))
{
    if (cpu == 0) {
        throw std::system_error(uniresis::uniresis_errc::failed_calculate_cpu_count);
    }

    if (mem == 0) {
        throw std::system_error(uniresis::uniresis_errc::failed_calculate_system_memory);
    }

    auto restrictions = args.as_object().at("restrictions", dynamic_t::empty_object).as_object();

    auto cpu_restricted = std::min(cpu, static_cast<uint>(restrictions.at("cpu", cpu).as_uint()));
    if (cpu != cpu_restricted) {
        cpu = cpu_restricted;
        COCAINE_LOG_INFO(log, "restricted available CPU count to {}", cpu);
    }

    auto mem_restricted = std::min(mem, static_cast<std::uint64_t>(restrictions.at("mem", mem).as_uint()));
    if (mem != mem_restricted) {
        mem = mem_restricted;
        COCAINE_LOG_INFO(log, "restricted available system memory to {}", mem);
    }

    on<io::uniresis::cpu_count>([&] {
        return cpu;
    });

    on<io::uniresis::memory_count>([&] {
        return mem;
    });
}

} // namespace service
} // namespace cocaine
