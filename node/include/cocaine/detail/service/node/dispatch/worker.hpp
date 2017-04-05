#pragma once

#include <cocaine/rpc/dispatch.hpp>

#include "cocaine/idl/node.hpp"
#include "cocaine/idl/rpc.hpp"

#include "cocaine/detail/service/node/forwards.hpp"

namespace cocaine {

using api::stream_t;

/// An adapter for [Client <- Worker] message passing.
class worker_rpc_dispatch_t:
    public dispatch<io::event_traits<io::worker::rpc::invoke>::dispatch_type>
{
public:
    typedef std::function<void(const std::error_code&)> callback_type;

private:
    typedef io::event_traits<io::worker::rpc::invoke>::upstream_type incoming_tag;
    typedef io::event_traits<io::app::enqueue>::upstream_type outcoming_tag;
    typedef io::protocol<incoming_tag>::scope protocol;

    std::shared_ptr<stream_t> stream;

    enum class state_t {
        open,
        closed
    };

    state_t state;

    /// On close callback.
    callback_type callback;

    std::mutex mutex;

public:
    /// \param stream rx stream provided from client.
    worker_rpc_dispatch_t(std::shared_ptr<stream_t> stream, callback_type callback);

    /// The worker has been disconnected without closing its opened channels.
    ///
    /// In this case we should notify all users about the failure.
    void
    discard(const std::error_code& ec) override;

private:
    void
    finalize(std::lock_guard<std::mutex>&, const std::error_code& ec = std::error_code());
};

} // namespace cocaine
