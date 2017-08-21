#pragma once

#include "cocaine/api/vicodyn/balancer.hpp"

#include "cocaine/service/node/slave/error.hpp"

#include <cocaine/errors.hpp>

namespace cocaine {
namespace vicodyn {
namespace balancer {

class simple_t: public api::vicodyn::balancer_t {
    dynamic_t args;

public:
    simple_t(context_t& ctx, asio::io_service& loop, const std::string& app_name, const dynamic_t& args);

    auto choose_peer(synchronized<proxy_t::mapping_t>& mapping, const hpack::headers_t& /*headers*/,
                     const std::string& /*event*/) -> std::shared_ptr<cocaine::vicodyn::peer_t> override;

    auto retry_count() -> size_t override;

    auto on_error(std::error_code, const std::string&) -> void override;

    auto is_recoverable(std::error_code ec, std::shared_ptr<peer_t> peer) -> bool override;

};

} // namespace balancer
} // namespace vicodyn
} // namespace cocaine
