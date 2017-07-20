#include "cocaine/service/uniresis.hpp"

#include <blackhole/logger.hpp>

#include <cocaine/api/unicorn.hpp>
#include <cocaine/context.hpp>
#include <cocaine/dynamic.hpp>

#include "cocaine/uniresis/error.hpp"

namespace cocaine {
namespace service {

/// A task that will try to notify about resources information on the machine.
class uniresis_t::updater_t {
    std::shared_ptr<api::unicorn_t> unicorn;

public:
    updater_t(std::shared_ptr<api::unicorn_t> unicorn) :
        unicorn(std::move(unicorn))
    {}

    auto
    notify() -> void {
        throw std::runtime_error("unimplemented");
    }
};

uniresis_t::uniresis_t(context_t& context, asio::io_service& loop, const std::string& name, const dynamic_t& args) :
    api::service_t(context, loop, name, args),
    dispatch<io::uniresis_tag>(name),
    resources(),
    updater(nullptr),
    log(context.log("uniresis"))
{
    if (resources.cpu == 0) {
        throw std::system_error(uniresis::uniresis_errc::failed_calculate_cpu_count);
    }

    if (resources.mem == 0) {
        throw std::system_error(uniresis::uniresis_errc::failed_calculate_system_memory);
    }

    auto restrictions = args.as_object().at("restrictions", dynamic_t::empty_object).as_object();

    auto cpu_restricted = std::min(
        resources.cpu,
        static_cast<uint>(restrictions.at("cpu", resources.cpu).as_uint())
    );

    if (resources.cpu != cpu_restricted) {
        resources.cpu = cpu_restricted;
        COCAINE_LOG_INFO(log, "restricted available CPU count to {}", resources.cpu);
    }

    auto mem_restricted = std::min(
        resources.mem,
        static_cast<std::uint64_t>(restrictions.at("mem", resources.mem).as_uint())
    );

    if (resources.mem != mem_restricted) {
        resources.mem = mem_restricted;
        COCAINE_LOG_INFO(log, "restricted available system memory to {}", resources.mem);
    }

    auto unicorn = api::unicorn(context, args.as_object().at("unicorn", "core").as_string());

    updater = std::make_shared<updater_t>(std::move(unicorn));

    on<io::uniresis::cpu_count>([&] {
        return resources.cpu;
    });

    on<io::uniresis::memory_count>([&] {
        return resources.mem;
    });
}

} // namespace service
} // namespace cocaine
