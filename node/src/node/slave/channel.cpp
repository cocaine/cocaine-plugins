#include "cocaine/detail/service/node/slave/channel.hpp"

#include "cocaine/detail/service/node/dispatch/client.hpp"
#include "cocaine/detail/service/node/dispatch/worker.hpp"

using namespace cocaine;

channel_t::channel_t(std::uint64_t id, time_point birthstamp, callback_type callback):
    id_(id),
    birthstamp_(birthstamp),
    callback(std::move(callback)),
    state(both),
    watched(false)
{}

std::uint64_t
channel_t::id() const noexcept {
    return id_;
}

channel_t::time_point
channel_t::birthstamp() const {
    return birthstamp_;
}

void
channel_t::close_send() {
    std::lock_guard<std::mutex> lock(mutex);
    state &= ~side_t::tx;
    maybe_notify(lock);
}

void
channel_t::close_recv() {
    std::lock_guard<std::mutex> lock(mutex);
    state &= ~side_t::rx;
    maybe_notify(lock);
}

void
channel_t::close_both() {
    std::lock_guard<std::mutex> lock(mutex);
    state &= ~(side_t::tx | side_t::rx);
    maybe_notify(lock);
}

bool
channel_t::closed() const {
    return state == side_t::none;
}

bool
channel_t::send_closed() const {
    return (state & side_t::tx) == side_t::tx;
}

bool
channel_t::recv_closed() const {
    return (state & side_t::rx) == side_t::rx;
}

void
channel_t::watch() {
    std::lock_guard<std::mutex> lock(mutex);
    watched = true;
    maybe_notify(lock);
}

void
channel_t::maybe_notify(std::lock_guard<std::mutex>&) {
    if (closed() && watched) {
        callback();
        into_worker.reset();
        from_worker.reset();
    }
}
