#include "cocaine/api/vicodyn/balancer.hpp"

namespace cocaine {
namespace api {
namespace vicodyn {

balancer_t::balancer_t(context_t&, cocaine::vicodyn::peers_t&, asio::io_service&, const std::string&,
                       const dynamic_t&, const dynamic_t::object_t&) {}

} // namespace vicodyn
} // namespace api
} // namespace cocaine
