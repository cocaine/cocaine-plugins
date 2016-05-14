#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

#include <cocaine/rpc/dispatch.hpp>

#include "cocaine/idl/rpc.hpp"

#include "cocaine/detail/service/node/forwards.hpp"
#include "cocaine/detail/service/node/rpc/slot.hpp"

namespace cocaine {

using detail::service::node::slave::control_t;

/// Initial dispatch for slaves.
///
/// Accepts only handshake messages and forwards it to the actual checker (i.e. to the Overseer).
/// This is a single-shot dispatch, it will be invalidated after the first handshake processed.
class handshaking_t:
    public dispatch<io::worker_tag>
{
    std::shared_ptr<session_t> session;

    std::mutex mutex;
    std::condition_variable cv;

public:
    template<class F>
    handshaking_t(const std::string& name, F&& fn):
        dispatch<io::worker_tag>(format("{}/handshake", name))
    {
        typedef io::streaming_slot<io::worker::handshake> slot_type;

        on<io::worker::handshake>(std::make_shared<slot_type>(
            [=](slot_type::upstream_type&& stream, const std::string& uuid) -> std::shared_ptr<control_t>
        {
            std::unique_lock<std::mutex> lock(mutex);
            // TODO: Perhaps we should use here `wait_for` to prevent forever waiting on slave, that
            // have been immediately killed.
            cv.wait(lock, [&]() -> bool {
                return !!session;
            });

            return fn(uuid ,std::move(session), std::move(stream));
        }));
    }

    void
    bind(std::shared_ptr<session_t> session) const {
        // Here we need that shitty const cast, because `dispatch_ptr_t` is a shared pointer over a
        // constant dispatch.
        const_cast<handshaking_t*>(this)->bind(std::move(session));
    }

    void
    bind(std::shared_ptr<session_t> session) {
        std::unique_lock<std::mutex> lock(mutex);
        this->session = std::move(session);
        lock.unlock();
        cv.notify_one();
    }
};

} // namespace cocaine
