#include <blackhole/logger.hpp>

#include "cocaine/detail/service/node/dispatch/client.hpp"
#include "cocaine/detail/service/node/dispatch/worker.hpp"
#include "cocaine/detail/service/node/slave/channel.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {

channel_t::channel_t(std::uint64_t id, time_point birthstamp, callback_type callback)
    : id_(id),
      birthstamp_(birthstamp),
      callback(std::move(callback)),
      state(both),
      watched(false) {}

auto channel_t::id() const noexcept -> std::uint64_t {
    return id_;
}

auto channel_t::birthstamp() const -> channel_t::time_point {
    return birthstamp_;
}

auto channel_t::close_send() -> void {
    std::lock_guard<std::mutex> lock(mutex);
    state &= ~side_t::tx;
    maybe_notify(lock);
}

auto channel_t::close_recv() -> void {
    std::lock_guard<std::mutex> lock(mutex);
    state &= ~side_t::rx;
    maybe_notify(lock);
}

auto channel_t::close_both() -> void {
    std::lock_guard<std::mutex> lock(mutex);
    state &= ~(side_t::tx | side_t::rx);
    maybe_notify(lock);
}

auto channel_t::closed() const -> bool {
    return state == side_t::none;
}

auto channel_t::send_closed() const -> bool {
    return (state & side_t::tx) == side_t::tx;
}

auto channel_t::recv_closed() const -> bool {
    return (state & side_t::rx) == side_t::rx;
}

auto channel_t::watch() -> void {
    std::lock_guard<std::mutex> lock(mutex);
    watched = true;
    maybe_notify(lock);
}

auto channel_t::maybe_notify(std::lock_guard<std::mutex>&) -> void {
    if (closed() && watched) {
        callback();
        into_worker.reset();
        from_worker.reset();
    }
}

}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
