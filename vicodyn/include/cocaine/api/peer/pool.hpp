#pragma once

#include "cocaine/vicodyn/forwards.hpp"

#include <cocaine/forwards.hpp>
#include <cocaine/rpc/graph.hpp>

#include <asio/ip/tcp.hpp>

namespace cocaine {
namespace api {
namespace peer {

class pool_t {
public:
    typedef pool_t category_type;
    virtual ~pool_t() = default;

    /// Process invocation inside pool. Peer selecting logic is usually applied before invocation.
    virtual
    auto invoke(const io::aux::decoded_message_t& incoming_message,
                const io::graph_node_t& protocol,
                io::upstream_ptr_t downstream) -> std::shared_ptr<cocaine::vicodyn::queue::send_t> = 0;

    virtual
    auto register_real(std::string uuid, std::vector<asio::ip::tcp::endpoint> endpoints, bool local) -> void = 0;

    virtual
    auto deregister_real(const std::string& uuid) -> void = 0;

    virtual
    auto size() -> size_t = 0;

    auto empty() -> bool {
        return size() == 0;
    }
};

typedef std::shared_ptr<pool_t> pool_ptr;

pool_ptr
pool(context_t& context, asio::io_service& io_loop, const std::string& pool_name, const std::string& service_name);

} // namespace peer
} // namespace api
} // namespace cocaine
