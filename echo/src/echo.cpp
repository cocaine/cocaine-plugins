#include "cocaine/service/echo.hpp"

namespace cocaine {
namespace service {

echo_t::echo_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args) :
    service_t(context, asio, name, args),
    dispatch<io::echo_tag>(name)
{
    on<io::echo::ping>([](const std::string& message) -> std::string {
        return message;
    });
}

}  // namespace service
}  // namespace cocaine
