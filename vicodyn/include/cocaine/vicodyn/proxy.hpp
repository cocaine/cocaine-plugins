#pragma once

#include "cocaine/vicodyn/forwards.hpp"
#include "cocaine/vicodyn/peer.hpp"

#include <cocaine/api/service.hpp>
#include <cocaine/executor/asio.hpp>
#include <cocaine/forwards.hpp>
#include <cocaine/rpc/dispatch.hpp>
#include <cocaine/idl/node.hpp>

#include <asio/ip/tcp.hpp>

namespace cocaine {
namespace vicodyn {

class proxy_t : public dispatch<io::app_tag> {
public:
    using event_t = io::app::enqueue;
    using slot_t = io::basic_slot<event_t>;
    using result_t = io::basic_slot<event_t>::result_type;
    using app_protocol = io::protocol<io::stream_of<std::string>::tag>::scope;
    using active_peers_t = std::vector<std::string>;

    friend class vicodyn_dispatch_t;

    proxy_t(context_t& context, asio::io_service& loop, peers_t& peers, const std::string& name,
            const dynamic_t& args, const dynamic_t::object_t& extra);

    auto empty() -> bool;

    auto size() -> size_t;

    auto choose_peer(const hpack::headers_t& headers, const std::string& event)
        -> std::shared_ptr<peer_t>;

private:
    auto make_balancer(const dynamic_t& args, const dynamic_t::object_t& extra) -> api::vicodyn::balancer_ptr;

    context_t& context;
    asio::io_service& loop;
    peers_t& peers;
    std::string app_name;
    api::vicodyn::balancer_ptr balancer;

    const std::unique_ptr<logging::logger_t> logger;
};

} // namespace vicodyn
} // namespace cocaine
