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

    friend class forward_dispatch_t;
    friend class backward_dispatch_t;

    proxy_t(context_t& context, peers_t& peers, const std::string& name, const dynamic_t& args);

    auto register_real(std::string uuid) -> void;

    auto deregister_real(const std::string& uuid) -> void;

    auto empty() -> bool;

    auto size() -> size_t;

    auto choose_peer(const hpack::headers_t& headers, const std::string& event)
        -> std::shared_ptr<peer_t>;

private:
    auto make_balancer(const dynamic_t& args) -> api::vicodyn::balancer_ptr;

    context_t& context;
    peers_t& peers;
    std::string app_name;
    executor::owning_asio_t executor;
    api::vicodyn::balancer_ptr balancer;

    const std::unique_ptr<logging::logger_t> logger;


    synchronized<active_peers_t> peers_with_app;
};

} // namespace vicodyn
} // namespace cocaine
