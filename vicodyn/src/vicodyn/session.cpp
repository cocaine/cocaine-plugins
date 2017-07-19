#include "cocaine/vicodyn/session.hpp"

namespace cocaine {
namespace vicodyn {

session_t::session_t(std::shared_ptr<cocaine::session_t> raw_session, connection_counter_t counter) :
    wrapped_session(std::move(raw_session)),
    connection_counter(std::move(counter))
{
    connection_counter->fetch_add(1);
}

session_t::~session_t() {
    wrapped_session->detach(std::error_code());
    connection_counter->fetch_add(-1);
}

auto session_t::shared(std::shared_ptr<cocaine::session_t> raw_session, connection_counter_t counter) -> std::shared_ptr<session_t> {
    return std::shared_ptr<session_t>(new session_t(std::move(raw_session), std::move(counter)));
}

auto session_t::get() const -> const cocaine::session_t& {
    return *wrapped_session;
}

auto session_t::get() -> cocaine::session_t& {
    return *wrapped_session;
}

} // namespace vicodyn
} // namespace cocaine
