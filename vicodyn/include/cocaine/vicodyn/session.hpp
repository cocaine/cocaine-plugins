#pragma once
#include "cocaine/vicodyn/forwards.hpp"

#include <cocaine/rpc/session.hpp>

#include <metrics/metric.hpp>

#include <memory>

namespace cocaine {
namespace vicodyn {

// wrapper that provides shared session ownership and detaches session in destructor
class session_t {
public:
    using connection_counter_t = metrics::shared_metric<std::atomic<std::uint64_t>>;
    ~session_t();
    session_t(const session_t&) = delete;
    session_t& operator=(const session_t&) = delete;

    static
    auto shared(std::shared_ptr<cocaine::session_t> raw_session, connection_counter_t counter)
            -> std::shared_ptr<session_t>;

    auto get() const -> const cocaine::session_t&;
    auto get() -> cocaine::session_t&;

private:
    session_t(std::shared_ptr<cocaine::session_t> raw_session, connection_counter_t counter);

    std::shared_ptr<cocaine::session_t> wrapped_session;
    connection_counter_t connection_counter;
};

} // namespace vicodyn
} // namespace cocaine
