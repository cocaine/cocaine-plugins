#pragma once

#include "cocaine/api/vicodyn/balancer.hpp"

#include "cocaine/service/node/slave/error.hpp"

#include <cocaine/errors.hpp>

#include <blackhole/logger.hpp>

namespace cocaine {
namespace vicodyn {
namespace balancer {

class simple_t: public api::vicodyn::balancer_t {
    peers_t& peers;
    std::unique_ptr<logging::logger_t> logger;
    dynamic_t args;
    std::string app_name;
    std::string x_cocaine_cluster;

public:
    simple_t(context_t& ctx, peers_t& peers, asio::io_service& loop, const std::string& app_name, const dynamic_t& args,
             const dynamic_t::object_t& locator_extra);

    auto choose_peer(const hpack::headers_t& /*headers*/, const std::string& /*event*/)
        -> std::shared_ptr<cocaine::vicodyn::peer_t> override;

    auto retry_count() -> size_t override;

    auto on_error(std::shared_ptr<peer_t>, std::error_code, const std::string&) -> void override;

    auto is_recoverable(std::shared_ptr<peer_t>, std::error_code ec) -> bool override;

};

} // namespace balancer
} // namespace vicodyn
} // namespace cocaine
