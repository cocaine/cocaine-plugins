#include "cocaine/api/vicodyn/balancer.hpp"
#include "cocaine/repository/vicodyn/balancer.hpp"

#include <cocaine/context.hpp>
#include <cocaine/context/config.hpp>
#include <cocaine/errors.hpp>

#include <boost/optional/optional.hpp>

namespace cocaine {
namespace api {
namespace vicodyn {

balancer_t::balancer_t(context_t&, asio::io_service&, const std::string&, const dynamic_t&) {
    // Empty
}

auto balancer(context_t& context, asio::io_service& io_service, const std::string& balancer_name, const std::string& service_name)
    -> balancer_ptr
{
    auto balancer = context.config().component_group("vicodyn").get(balancer_name);
    if (!balancer) {
        throw error_t(error::component_not_found, "vicodyn component with name '{}' not found", balancer_name);
    }
    return context.repository().get<balancer_t>(balancer->type(), context, io_service, service_name, balancer->args());
}

} // namespace vicodyn
} // namespace api
} // namespace cocaine
