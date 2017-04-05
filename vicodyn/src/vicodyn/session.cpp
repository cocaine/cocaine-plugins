#include "cocaine/vicodyn/session.hpp"

namespace cocaine {
namespace vicodyn {

session_t::session_t(std::shared_ptr<cocaine::session_t> raw_session) :
    wrapped_session(std::move(raw_session))
{
}

session_t::~session_t() {
    //TODO: log?
    VICODYN_DEBUG("destroying session");
    wrapped_session->detach(std::error_code());
}

auto session_t::shared(std::shared_ptr<cocaine::session_t> raw_session) -> std::shared_ptr<session_t> {
    return std::shared_ptr<session_t>(new session_t(raw_session));
}

auto session_t::get() const -> const cocaine::session_t& {
    return *wrapped_session;
}

auto session_t::get() -> cocaine::session_t& {
    return *wrapped_session;
}

} // namespace vicodyn
} // namespace cocaine
