#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <system_error>

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {

class channel_t : public std::enable_shared_from_this<channel_t> {
public:
    typedef std::function<void()> callback_type;
    typedef std::chrono::high_resolution_clock::time_point time_point;

private:
    enum side_t { none = 0x00, tx = 0x01, rx = 0x02, both = tx | rx };

    const std::uint64_t id_;
    const time_point birthstamp_;
    const callback_type callback;

    std::atomic<int> state;
    bool watched;
    std::mutex mutex;

public:
    std::shared_ptr<const client_rpc_dispatch_t> into_worker;
    std::shared_ptr<const worker_rpc_dispatch_t> from_worker;

public:
    channel_t(std::uint64_t id, time_point birthstamp, callback_type callback);

    auto id() const noexcept -> std::uint64_t;

    auto birthstamp() const -> time_point;

    auto closed() const -> bool;

    auto send_closed() const -> bool;
    auto recv_closed() const -> bool;

    auto watch() -> void;

    auto close_send() -> void;
    auto close_recv() -> void;
    auto close_both() -> void;

private:
    auto maybe_notify(std::lock_guard<std::mutex>& lock) -> void;
};

}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
