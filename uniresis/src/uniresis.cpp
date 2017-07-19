#include "cocaine/service/uniresis.hpp"

namespace cocaine {
namespace service {

uniresis_t::uniresis_t(context_t& context, asio::io_service& loop, const std::string& name, const dynamic_t& args) :
    api::service_t(context, loop, name, args),
    dispatch<io::uniresis_tag>(name)
{}

} // namespace service
} // namespace cocaine
