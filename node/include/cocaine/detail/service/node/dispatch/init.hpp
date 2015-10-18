#pragma once

#include <string>

#include <cocaine/rpc/dispatch.hpp>

#include "cocaine/idl/rpc.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {

/// The basic prototype.
///
/// It's here only, because Cocaine API wants it in actors. Does nothing, because it is always
/// replaced by a handshake dispatch for every incoming connection.
class init_dispatch_t : public dispatch<io::worker_tag> {
public:
    init_dispatch_t(const std::string& name) : dispatch<io::worker_tag>(name) {}
};

}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
