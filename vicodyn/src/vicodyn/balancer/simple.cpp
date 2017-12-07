#include "cocaine/vicodyn/balancer/simple.hpp"

#include <cocaine/context.hpp>

namespace cocaine {
namespace vicodyn {
namespace balancer {

template<class Iterator, class Predicate>
auto choose_random_if(Iterator begin, Iterator end, size_t size, Predicate p) -> Iterator {
    std::vector<Iterator> chosen;
    chosen.reserve(size);
    while(begin != end) {
        if(p(*begin)) {
            chosen.push_back(begin);
        }
        begin++;
    }
    if(chosen.empty()) {
        return end;
    }
    return chosen[rand() % chosen.size()];
}

simple_t::simple_t(context_t& ctx, peers_t& peers, asio::io_service& loop, const std::string& app_name,
                   const dynamic_t& args, const dynamic_t::object_t& locator_extra) :
    api::vicodyn::balancer_t(ctx, peers, loop, app_name, args, locator_extra),
    peers(peers),
    logger(ctx.log(format("balancer/simple/{}", app_name))),
    args(args),
    app_name(app_name),
    x_cocaine_cluster(locator_extra.at("x-cocaine-cluster", "").as_string())
{
    COCAINE_LOG_INFO(logger, "created simple balancer for app {}", app_name);
}

auto simple_t::choose_peer(const std::shared_ptr<request_context_t>& /*request_context*/, const hpack::headers_t& /*headers*/,
                           const std::string& /*event*/) -> std::shared_ptr<cocaine::vicodyn::peer_t>
{
    return peers.apply_shared([&](const peers_t::data_t& mapping) {
        auto apps_it = mapping.apps.find(app_name);
        if(apps_it == mapping.apps.end() || apps_it->second.empty()) {
            COCAINE_LOG_WARNING(logger, "peer list for app {} is empty", app_name);
            throw error_t("no peers found");
        }
        auto& apps = apps_it->second;
        auto it = choose_random_if(mapping.peers.begin(), mapping.peers.end(), mapping.peers.size(),
            [&](const peers_t::peers_data_t::value_type pair) -> bool {
                if(!pair.second->connected()) {
                    return false;
                }
                if(x_cocaine_cluster != pair.second->x_cocaine_cluster()) {
                    return false;
                }
                return apps.count(pair.second->uuid());
            }
        );
        if(it != mapping.peers.end()) {
            return it->second;
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
