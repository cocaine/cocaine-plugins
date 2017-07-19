#include "cocaine/service/uniresis.hpp"

#include <thread>

#include <blackhole/logger.hpp>

#include <cocaine/context.hpp>
#include <cocaine/dynamic.hpp>

#include "cocaine/uniresis/error.hpp"

namespace cocaine {
namespace service {

uniresis_t::uniresis_t(context_t& context, asio::io_service& loop, const std::string& name, const dynamic_t& args) :
    api::service_t(context, loop, name, args),
    dispatch<io::uniresis_tag>(name),
    cpu(std::thread::hardware_concurrency()),
    log(context.log("uniresis"))
{
    if (cpu == 0) {
        throw std::system_error(uniresis::uniresis_errc::failed_calculate_cpu_count);
    }

    auto restrictions = args.as_object().at("restrictions", dynamic_t::empty_object).as_object();

    auto cpu_restricted = std::min(cpu, static_cast<uint>(restrictions.at("cpu", cpu).as_uint()));
    if (cpu != cpu_restricted) {
        cpu = cpu_restricted;
        COCAINE_LOG_INFO(log, "restricted available CPU count to {}", cpu);
    }

    on<io::uniresis::cpu_count>([&] {
        return cpu;
    });
}

} // namespace service
} // namespace cocaine
