#pragma once

#include "cocaine/vicodyn/forwards.hpp"
#include "cocaine/vicodyn/proxy.hpp"

#include <cocaine/forwards.hpp>
#include <cocaine/rpc/graph.hpp>

#include <asio/ip/tcp.hpp>

namespace cocaine {
namespace api {
namespace vicodyn {

class balancer_t {
public:
    using category_type = balancer_t;

    virtual
    ~balancer_t() = default;

    balancer_t(context_t& context, cocaine::vicodyn::peers_t& peers, asio::io_service& io_service,
               const std::string& app_name, const dynamic_t& conf, const dynamic_t::object_t& locator_extra);

    virtual
    auto choose_peer(const std::shared_ptr<cocaine::vicodyn::request_context_t>& request_context,
                     const hpack::headers_t& headers, const std::string& event) -> std::shared_ptr<cocaine::vicodyn::peer_t> = 0;

    virtual
    auto retry_count() -> size_t = 0;

    virtual
    auto on_error(std::shared_ptr<cocaine::vicodyn::peer_t> peer, std::error_code ec, const std::string& msg) -> void = 0;

    virtual
    auto is_recoverable(std::shared_ptr<cocaine::vicodyn::peer_t> peer, std::error_code ec) -> bool = 0;
};

} // namespace peer
} // namespace api
} // namespace cocaine
