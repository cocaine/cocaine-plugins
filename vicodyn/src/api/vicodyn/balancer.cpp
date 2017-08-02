#include "cocaine/api/vicodyn/balancer.hpp"
#include "cocaine/repository/vicodyn/balancer.hpp"

#include <cocaine/context.hpp>
#include <cocaine/context/config.hpp>
#include <cocaine/errors.hpp>

#include <boost/optional/optional.hpp>
#include <cocaine/dynamic.hpp>

namespace cocaine {
namespace api {
namespace vicodyn {

balancer_t::balancer_t(context_t&, asio::io_service&, const std::string&, const dynamic_t&) {
    // Empty
}

auto balancer(context_t& context, asio::io_service& io_service, const dynamic_t& balancer_args, const std::string& service_name)
    -> balancer_ptr
{
    auto name = balancer_args.as_object().at("type", "simple").as_string();
    auto args = balancer_args.as_object().at("args", dynamic_t::empty_object);
    return context.repository().get<balancer_t>(name, context, io_service, service_name, args);
}

} // namespace vicodyn
} // namespace api
} // namespace cocaine
