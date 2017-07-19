#include "cocaine/service/uniresis.hpp"

#include <thread>

#include "cocaine/uniresis/error.hpp"

namespace cocaine {
namespace service {

uniresis_t::uniresis_t(context_t& context, asio::io_service& loop, const std::string& name, const dynamic_t& args) :
    api::service_t(context, loop, name, args),
    dispatch<io::uniresis_tag>(name),
    cpu(std::thread::hardware_concurrency())
{
    if (cpu == 0) {
        throw std::system_error(uniresis::uniresis_errc::failed_calculate_cpu_count);
    }

    on<io::uniresis::cpu_count>([&] {
        return cpu;
    });
}

} // namespace service
} // namespace cocaine
