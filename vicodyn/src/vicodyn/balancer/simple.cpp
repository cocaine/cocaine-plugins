#include "cocaine/vicodyn/balancer/simple.hpp"

#include <cocaine/context.hpp>

namespace cocaine {
namespace vicodyn {
namespace balancer {

simple_t::simple_t(context_t& ctx, peers_t& peers, asio::io_service& loop, const std::string& app_name, const dynamic_t& args) :
    api::vicodyn::balancer_t(ctx, peers, loop, app_name, args),
    peers(peers),
    logger(ctx.log(format("balancer/simple/{}", app_name))),
    args(args),
    app_name(app_name)
{
    COCAINE_LOG_INFO(logger, "created simple balancer for app {}", app_name);
}

auto simple_t::choose_peer(const hpack::headers_t& /*headers*/, const std::string& /*event*/)
    -> std::shared_ptr<cocaine::vicodyn::peer_t>
{
    return peers.inner().apply([&](peers_t::data_t& mapping) {
        auto& apps = mapping.apps[app_name];
        auto sz = apps.size();
        if(sz == 0) {
            COCAINE_LOG_WARNING(logger, "peer list for app {} is empty", app_name);
            throw error_t("no peers found");
        }
        auto idx = std::rand() % sz;
        for (size_t i = 0; i < sz; i++){
            const auto& uuid = apps[idx];
            auto it = mapping.peers.find(uuid);
            if(it != mapping.peers.end()) {
                return it->second;
            }
            idx++;
            if(idx >= sz) {
                idx = 0;
            }
        }
        COCAINE_LOG_WARNING(logger, "all peers do not have desired app");
        throw error_t("no peers found");
    });
}

auto simple_t::retry_count() -> size_t {
    return args.as_object().at("retry_count", 10u).as_uint();
}

auto simple_t::on_error(std::shared_ptr<peer_t> peer, std::error_code ec, const std::string& msg) -> void {
    COCAINE_LOG_WARNING(logger, "peer errored - {}({})", ec.message(), msg);
    if(ec.category() == error::node_category() && ec.value() == error::node_errors::not_running) {
        peers.erase_app(peer->uuid(), app_name);
    }
}

auto simple_t::is_recoverable(std::shared_ptr<peer_t>, std::error_code ec) -> bool {
    bool queue_is_full = (ec.category() == error::overseer_category() && ec.value() == error::queue_is_full);
    bool unavailable = (ec.category() == error::node_category() && ec.value() == error::not_running);
    bool disconnected = (ec.category() == error::dispatch_category() && ec.value() == error::not_connected);
    return queue_is_full || unavailable || disconnected;
}

} // namespace balancer
} // namespace vicodyn
} // namespace cocaine
