#pragma once
#include "cocaine/vicodyn/forwards.hpp"

#include <cocaine/rpc/session.hpp>

#include <memory>

namespace cocaine {
namespace vicodyn {

// wrapper that provides shared session ownership and detaches session in destructor
class session_t {
public:
    ~session_t();
    session_t(const session_t&) = delete;
    session_t& operator=(const session_t&) = delete;

    static auto shared(std::shared_ptr<cocaine::session_t> raw_session) -> std::shared_ptr<session_t>;
    auto get() const -> const cocaine::session_t&;
    auto get() -> cocaine::session_t&;

private:
    session_t(std::shared_ptr<cocaine::session_t> raw_session);

    std::shared_ptr<cocaine::session_t> wrapped_session;
};

} // namespace vicodyn
} // namespace cocaine
