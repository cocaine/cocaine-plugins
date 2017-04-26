#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

#include <cocaine/rpc/dispatch.hpp>

#include "cocaine/idl/rpc.hpp"

#include "cocaine/detail/service/node/forwards.hpp"

namespace cocaine {

using detail::service::node::slave::control_t;

template<class Event, class F>
class forward_slot : public io::basic_slot<Event> {
public:
    typedef Event event_type;

    typedef std::vector<hpack::header_t> meta_type;
    typedef typename io::basic_slot<event_type>::tuple_type    tuple_type;
    typedef typename io::basic_slot<event_type>::dispatch_type dispatch_type;
    typedef typename io::basic_slot<event_type>::upstream_type upstream_type;

private:
    const F f;

public:
    forward_slot(F f) : f(std::move(f)) {}

    virtual
    boost::optional<std::shared_ptr<dispatch_type>>
    operator()(const meta_type& headers, tuple_type&& args, upstream_type&& upstream) {
        return f(std::move(upstream), headers, std::move(args));
    }
};

template<class Event, class F>
auto make_forward_slot(F&& f) -> std::shared_ptr<io::basic_slot<Event>> {
    return std::make_shared<forward_slot<Event, F>>(std::forward<F>(f));
}

/// Initial dispatch for slaves.
///
/// Accepts only handshake messages and forwards it to the actual checker (i.e. to the Overseer).
/// This is a single-shot dispatch, it will be invalidated after the first handshake processed.
class handshaking_t:
    public dispatch<io::worker_tag>
{
    std::shared_ptr<session_t> session;

public:
    template<class F>
    handshaking_t(const std::string& name, F fn):
        dispatch<io::worker_tag>(format("{}/handshake", name))
    {
        auto handler = [=](
            upstream<io::worker::control_tag>&& upstream,
            const std::vector<hpack::header_t>&,
            std::tuple<std::string> args) -> std::shared_ptr<dispatch<io::worker::control_tag>>
        {
            std::string uuid;
            std::tie(uuid) = args;
            return fn(std::move(uuid), std::move(session), std::move(upstream));
        };

        on<io::worker::handshake>(make_forward_slot<io::worker::handshake>(std::move(handler)));
    }

    auto
    attached(std::shared_ptr<session_t> session) -> void {
        this->session = std::move(session);
    }
};

} // namespace cocaine
