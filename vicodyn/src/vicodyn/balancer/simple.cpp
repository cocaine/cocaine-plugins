#include "cocaine/vicodyn/balancer/simple.hpp"

namespace cocaine {
namespace vicodyn {
namespace balancer {

simple_t::simple_t(context_t& ctx, asio::io_service& loop, const std::string& app_name, const dynamic_t& args) :
    api::vicodyn::balancer_t(ctx, loop, app_name, args),
    args(args)
{}

auto simple_t::choose_peer(synchronized<proxy_t::mapping_t>& mapping, const hpack::headers_t& /*headers*/,
                           const std::string& /*event*/) -> std::shared_ptr<cocaine::vicodyn::peer_t>
{
    return mapping.apply([&](proxy_t::mapping_t& mapping) {
        auto sz = mapping.peers_with_app.size();
        if(sz == 0) {
            throw error_t("no peers found");
        }
        auto idx = std::rand() % sz;
        for (size_t i = 0; i < sz; i++){
            const auto& uuid = mapping.peers_with_app[idx];
            auto it = mapping.node_peers.find(uuid);
            if(it != mapping.node_peers.end()) {
                return it->second;
            }
            idx++;
            if(idx >= sz) {
                idx = 0;
            }
        }
        throw error_t("no peers found");
    });
}

auto simple_t::retry_count() -> size_t {
    return args.as_object().at("retry_count", 10u).as_uint();
}

auto simple_t::on_error(std::error_code, const std::string&) -> void {
    // no op
}

auto simple_t::is_recoverable(std::error_code ec, std::shared_ptr<peer_t> /*peer*/) -> bool {
    bool queue_is_full = (ec.category() == error::overseer_category() && ec.value() == error::queue_is_full);
    bool unavailable = (ec.category() == error::node_category() && ec.value() == error::not_running);
    return queue_is_full || unavailable;
}

} // namespace balancer
} // namespace vicodyn
} // namespace cocaine
