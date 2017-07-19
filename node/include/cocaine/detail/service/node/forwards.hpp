#pragma once

#include "cocaine/service/node/forwards.hpp"

namespace cocaine {

class session_t;

class client_rpc_dispatch_t;
class worker_rpc_dispatch_t;

namespace api {

struct cancellation_t;
struct spool_handle_base_t;
struct spawn_handle_base_t;
struct metrics_handle_base_t;

class stream_t;

}  // namespace api

namespace detail {
namespace service {
namespace node {
namespace slave {

class channel_t;
class control_t;
class fetcher_t;
class machine_t;
class spawn_handle_t;

struct load_t;
struct stats_t;

namespace state {

class active_t;
class preparation_t;
class handshaking_t;
class inactive_t;
class seal_t;
class spawn_t;
class state_t;
class terminate_t;

}  // namespace state
}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
