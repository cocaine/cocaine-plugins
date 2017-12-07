#pragma once

#include <memory>

namespace cocaine {
namespace api {
namespace vicodyn {

    class balancer_t;
    using balancer_ptr = std::shared_ptr<balancer_t>;

} // namespace vicodyn
} // namespace api
} // namespace cocaine

namespace cocaine {
namespace gateway {

class vicodyn_t;

} // namespace service
} // namespace cocaine

namespace cocaine {
namespace vicodyn {

class dispatch_t;
class request_context_t;

} // namespace vicodyn
} // namespace cocaine

namespace cocaine {
namespace vicodyn {

class invocation_t;
class proxy_t;
class peer_t;

} // namespace vicodyn
} // namespace cocaine
