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

    friend class forward_dispatch_t;

    struct mapping_t {
        std::map<std::string, std::shared_ptr<peer_t>> node_peers;
        std::vector<std::string> peers_with_app;
    };

    proxy_t(context_t& context, const std::string& name, const dynamic_t& args);

    auto register_node(const std::string& uuid, std::vector<asio::ip::tcp::endpoint> endpoints) -> void;

    auto deregister_node(const std::string& uuid) -> void;

    auto register_real(std::string uuid) -> void;

    auto deregister_real(const std::string& uuid) -> void;

    auto empty() -> bool;

    auto size() -> size_t;

    auto choose_peer(const hpack::headers_t& headers, const std::string& event)
        -> std::shared_ptr<peer_t>;

private:
    auto make_balancer(const dynamic_t& args) -> api::vicodyn::balancer_ptr;

    context_t& context;
    std::string app_name;
    executor::owning_asio_t executor;
    api::vicodyn::balancer_ptr balancer;

    const std::unique_ptr<logging::logger_t> logger;

    synchronized<mapping_t> mapping;
};

} // namespace vicodyn
} // namespace cocaine
