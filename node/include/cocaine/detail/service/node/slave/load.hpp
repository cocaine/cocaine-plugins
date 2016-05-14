#pragma once

#include <memory>

#include <cocaine/trace/trace.hpp>

#include "cocaine/service/node/app/event.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {

using cocaine::service::node::app::event_t;

struct load_t {
    /// Event to be processed.
    event_t event;

    /// Associcated trace.
    trace_t trace;

    /// Optional slave id to be able to route events into the same slaves.
    boost::optional<id_t> id;

    /// An TX dispatch.
    std::shared_ptr<client_rpc_dispatch_t> dispatch;

    /// An RX stream provided from user. The slave will call its callbacks on every incoming event.
    std::shared_ptr<api::stream_t> downstream;
};

}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
