#pragma once

namespace cocaine {

struct manifest_t;
struct profile_t;

// TODO: Move.
class client_rpc_dispatch_t;

namespace detail {
namespace service {
namespace node {

class metrics_retriever_t;
class engine_t;

namespace slave {

class machine_t;

}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail

namespace api {

class stream_t;

}  // namespace api

namespace service {
namespace node {

class app_t;
class overseer_t;

// Reexport.
using detail::service::node::engine_t;

namespace slave {

class id_t;

// Reexport.
using detail::service::node::slave::machine_t;

}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace cocaine
