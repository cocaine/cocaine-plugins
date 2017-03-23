#pragma once

#include <functional>
#include <future>
#include <string>

#include <boost/thread/thread.hpp>

#include "cocaine/common.hpp"
#include "cocaine/idl/node.hpp"
#include "cocaine/rpc/slot/deferred.hpp"

#include "cocaine/service/node/forwards.hpp"

namespace cocaine { namespace service { namespace node {
class app_state_t;
}}} // namespace cocaine::service::node

namespace cocaine { namespace service { namespace node {

/// Represents a single application.
///
/// Starts TCP and UNIX servers.
class app_t {
    COCAINE_DECLARE_NONCOPYABLE(app_t)

private:
    std::shared_ptr<asio::io_service> loop;
    std::unique_ptr<asio::io_service::work> work;
    std::unique_ptr<boost::thread> thread;
    std::shared_ptr<app_state_t> state;

public:
    app_t(context_t& context,
          const std::string& manifest,
          const std::string& profile,
          std::function<void(std::future<void> future)> callback);
    ~app_t();

    std::string
    name() const;

    dynamic_t
    info(io::node::info::flags_t flags) const;

    std::shared_ptr<overseer_t>
    overseer() const;
};

}}} // namespace cocaine::service::node
