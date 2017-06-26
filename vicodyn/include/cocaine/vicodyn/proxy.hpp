#pragma once

#include "cocaine/vicodyn/pool.hpp"

#include <cocaine/api/service.hpp>
#include <cocaine/forwards.hpp>
#include <cocaine/rpc/basic_dispatch.hpp>

namespace cocaine {
namespace vicodyn {

class proxy_t : public io::basic_dispatch_t {
public:
    proxy_t(context_t& context,
            std::shared_ptr<asio::io_service> io_loop,
            const std::string& name,
            const dynamic_t& args,
            unsigned int version,
            io::graph_root_t protocol);

    auto root() const -> const io::graph_root_t& override {
        return m_protocol;
    }

    auto version() const -> int override {
        return m_version;
    }

    auto process(const io::decoder_t::message_type& incoming_message, const io::upstream_ptr_t& backward_stream) ->
        boost::optional<io::dispatch_ptr_t> override;

    auto register_real(std::string uuid, std::vector<asio::ip::tcp::endpoint> endpoints, bool local) -> void {
        pool.register_real(uuid, endpoints, local);
    }

    auto deregister_real(const std::string& uuid) -> void {
        pool.deregister_real(uuid);
    }

    auto empty() -> bool {
        return pool.size() == 0;
    }

    auto size() -> size_t {
        return pool.size();
    }

private:
    std::shared_ptr<asio::io_service> io_loop;
    const std::unique_ptr<logging::logger_t> logger;
    const io::graph_root_t m_protocol;
    const unsigned int m_version;
    pool_t pool;
};

} // namespace vicodyn
} // namespace cocaine
