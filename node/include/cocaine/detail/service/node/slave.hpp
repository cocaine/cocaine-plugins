#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <system_error>

#include <cocaine/forwards.hpp>

#include "cocaine/api/auth.hpp"
#include "cocaine/idl/rpc.hpp"

#include "cocaine/detail/service/node/forwards.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {

using detail::service::node::slave::load_t;
using detail::service::node::slave::channel_t;
using detail::service::node::slave::control_t;
using detail::service::node::slave::fetcher_t;
using detail::service::node::slave::state::state_t;

using cocaine::service::node::slave::id_t;
using cocaine::service::node::slave::machine_t;

// TODO: Rename to `comrade`, because in Soviet Russia slave owns you!
class slave_t {
public:
    typedef std::function<void(const std::error_code& ec)> cleanup_handler;

private:
    /// Termination reason.
    std::error_code ec;

    struct {
        std::string id;
        std::chrono::high_resolution_clock::time_point birthstamp;
    } data;

    /// The slave state machine implementation.
    std::shared_ptr<machine_t> machine;

public:
    slave_t(context_t& context,
            id_t id,
            manifest_t manifest,
            profile_t profile,
            std::shared_ptr<api::auth_t> auth,
            asio::io_service& loop,
            cleanup_handler fn);
    slave_t(const slave_t& other) = delete;
    slave_t(slave_t&&) = default;

    ~slave_t();

    slave_t& operator=(const slave_t& other) = delete;
    slave_t& operator=(slave_t&&) = default;

    // Observers.

    const std::string&
    id() const noexcept;

    long long
    uptime() const;

    std::uint64_t
    load() const;

    detail::service::node::slave::stats_t
    stats() const;

    bool
    active() const noexcept;

    /// Returns the profile attached.
    profile_t
    profile() const;

    // Modifiers.

    std::shared_ptr<control_t>
    activate(std::shared_ptr<session_t> session, upstream<io::worker::control_tag> stream);

    auto inject(load_t& load, std::function<void(std::uint64_t)> handler) -> std::uint64_t;

    void
    seal();

    /// Marks the slave for termination using the given error code.
    ///
    /// It will be terminated later in destructor.
    void
    terminate(std::error_code ec);
};

}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
