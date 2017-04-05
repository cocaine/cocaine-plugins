#pragma once

#include "cocaine/vicodyn/debug.hpp"

namespace cocaine {
namespace api {
namespace peer {

class pool_t;

} // namespace peer
} // namespace api
} // namespace cocaine

namespace cocaine {
namespace gateway {

class vicodyn_t;

} // namespace service
} // namespace cocaine


namespace cocaine {
namespace vicodyn {
namespace proxy {

struct appendable_t;
class dispatch_t;

} // namespace proxy
} // namespace vicodyn
} // namespace cocaine

namespace cocaine {
namespace vicodyn {
namespace queue {

class invocation_t;
class send_t;

} // namespace queue
} // namespace vicodyn
} // namespace cocaine

namespace cocaine {
namespace vicodyn {
namespace stream {

class wrapper_t;

} // namespace stream
} // namespace vicodyn
} // namespace cocaine

namespace cocaine {
namespace vicodyn {
namespace proxy {

struct appendable_t;
class dispatch_t;

} // namespace proxy
} // namespace vicodyn
} // namespace cocaine

namespace cocaine {
namespace vicodyn {

class proxy_t;
class peer_t;
class stream_t;
class session_t;
using stream_ptr_t = std::shared_ptr<stream_t>;

} // namespace vicodyn
} // namespace cocaine

namespace msgpack {

class unpacked;
struct object;

} // namespace msgpack
