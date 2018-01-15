#pragma once

#include <chrono>
#include <memory>
#include <unordered_map>

#include <blackhole/logger.hpp>
#include <blackhole/scope/holder.hpp>

#include <cocaine/format.hpp>
#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/repository.hpp>

#include "cocaine/service/node/manifest.hpp"
#include "cocaine/service/node/profile.hpp"

#include "cocaine/api/isolate.hpp"

#include <metrics/factory.hpp>
#include <metrics/registry.hpp>

#include "node/pool_observer.hpp"

namespace cocaine {
namespace detail {
namespace service {
namespace node {

class engine_t;

namespace conf {
    constexpr auto metrics_poll_interval_s = 5u;
}

// TODO: Refactor: make small names.
enum class aggregate_t : unsigned {
        instant,   // treat value `as is`
        aggregate, // isolate returns accumalated value on each request (ioread, etc)
};

struct metrics_aggregate_proxy_t;

struct worker_metrics_t {
    // <not monotonic (instant); shared_metric; delta from prev. value (for monotonic increasing value)>
    struct counter_metric_t {
        using value_type = std::uint64_t;

        aggregate_t aggregation;
        metrics::shared_metric<std::atomic<value_type>> value;
        value_type delta; // if is_accumulated = true then delta = value - prev(value), used for app-wide aggration
    };

    struct network_metrics_t {
        using value_type = std::uint64_t;

        metrics::shared_metric<std::atomic<value_type>> rx_bytes;
        metrics::shared_metric<std::atomic<value_type>> tx_bytes;
    };

    using clock_type = std::chrono::system_clock;
    using gauge_repr_type = double;
    using gauge_metrics_type = metrics::shared_metric<metrics::gauge<gauge_repr_type>>;

    std::unordered_map<std::string, counter_metric_t> common_counters;
    std::unordered_map<std::string, gauge_metrics_type> gauges;
    std::unordered_map<std::string, network_metrics_t> network;

    //
    // Context ref is needed to register network metrics on demand.
    //
    // TODO: Danger zone! Ensure that `worker_metrics_t` will not outlive
    //       context someway.
    //
    context_t& ctx;

    clock_type::time_point update_stamp;
    std::string name_prefix;

    worker_metrics_t(context_t& ctx, const std::string& name_prefix);
    worker_metrics_t(context_t& ctx, const std::string& app_name, const std::string& id);

    friend auto
    operator+(worker_metrics_t& src, metrics_aggregate_proxy_t& proxy) -> metrics_aggregate_proxy_t&;

    auto
    assign(metrics_aggregate_proxy_t&& init) -> void;

    template<typename Integral>
    auto
    update_counter(const std::string& name, const aggregate_t desired, const Integral value = 0) -> void;

    template<typename Fn>
    auto
    update_gauge(const std::string& name, Fn&& fun) -> void;
};

struct metrics_aggregate_proxy_t {
    // see worker_metrics_t::counter_metric_t
    struct counter_metric_t {
        using value_type = worker_metrics_t::counter_metric_t::value_type;

        // TODO: union?
        aggregate_t type;
        value_type values; // summation of worker_metrics values
        value_type deltas; // summation of worker_metrics deltas
    };

    struct gauge_avg_t {
        using value_type = worker_metrics_t::gauge_repr_type;

        value_type value;
        int count;

        auto add(const value_type value) -> void {
            this->value += value;
            ++this->count;
        }
    };

    std::unordered_map<std::string, counter_metric_t> common_counters;
    std::unordered_map<std::string, gauge_avg_t> gauges;

    auto
    operator+(const worker_metrics_t& worker_metrics) -> metrics_aggregate_proxy_t&;
};

/// Isolation daemon's workers metrics sampler.
///
/// Poll sequence should be initialized explicitly with
/// metrics_retriever_t::ignite_poll method or implicitly
/// within metrics_retriever_t::make_and_ignite.
class metrics_retriever_t :
    public std::enable_shared_from_this<metrics_retriever_t>
{
public:
    using stats_table_type = std::unordered_map<std::string, worker_metrics_t>;
private:
    context_t& context;

    asio::deadline_timer metrics_poll_timer;
    std::shared_ptr<api::isolate_t> isolate;

    std::weak_ptr<engine_t> parent_engine;

    const std::unique_ptr<cocaine::logging::logger_t> log;

    //
    // Poll intervals could be quite large (should be configurable), so the
    // Isolation Daemon supports metrics `in memory` persistance for dead workers
    // (at least for 30 seconds) and it will be possible to query for workers which
    // have passed away not too long ago. Their uuids will be taken and added to
    // request from `purgatory`.
    //
    using purgatory_pot_type = std::set<std::string>;
    synchronized<purgatory_pot_type> purgatory;

    // `synchronized` not needed within current design, but shouldn't do any harm
    // <uuid, metrics>
    synchronized<stats_table_type> metrics;

    struct self_metrics_t {
        metrics::shared_metric<std::atomic<std::uint64_t>> uuids_requested;
        metrics::shared_metric<std::atomic<std::uint64_t>> uuids_recieved;
        metrics::shared_metric<std::atomic<std::uint64_t>> requests_send;
        metrics::shared_metric<std::atomic<std::uint64_t>> empty_requests;
        metrics::shared_metric<std::atomic<std::uint64_t>> responses_received;
        metrics::shared_metric<std::atomic<std::uint64_t>> receive_errors;
        metrics::shared_metric<std::atomic<std::uint64_t>> posmortem_queue_size;
        self_metrics_t(context_t& ctx, const std::string& name);
    } self_metrics;

    std::string app_name;
    boost::posix_time::seconds poll_interval;

    worker_metrics_t app_aggregate_metrics;
public:

    metrics_retriever_t(
        context_t& ctx,
        const std::string& name,
        std::shared_ptr<api::isolate_t> isolate,
        const std::shared_ptr<engine_t>& parent_engine,
        asio::io_service& loop,
        const std::uint64_t poll_interval);

    ///
    /// Reads following section from Cocaine-RT configuration:
    ///  ```
    ///  "node" : {
    ///     ...
    ///     "args" : {
    ///        "isolate_metrics:" : true,
    ///        "isolate_metrics_poll_period_s" : 10
    ///     }
    ///  }
    ///  ```
    /// if `isolate_metrics` is false (default), returns nullptr,
    /// and polling sequence wouldn't start.
    ///
    // TODO: it seems that throw on construction error would be a better choice.
    template<typename Observers>
    static
    auto
    make_and_ignite(
        context_t& ctx,
        const std::string& name,
        std::shared_ptr<api::isolate_t> isolate,
        const std::shared_ptr<engine_t>& parent_engine,
        asio::io_service& loop,
        synchronized<Observers>& observers) -> std::shared_ptr<metrics_retriever_t>;

    auto
    ignite_poll() -> void;

    auto
    make_observer() -> std::shared_ptr<cocaine::service::node::pool_observer>;

    //
    // Should be called on every pool::erase(id) invocation
    //
    // usually isolation daemon will hold metrics of despawned workers for some
    // reasonable period (at least 30 sec), so it is allowed to request metrics
    // of dead workers on next `poll` invocation.
    //
    // Note: on high despawn rates stat of some unlucky workers will be lost
    //
    auto
    add_post_mortem(const std::string& id) -> void;

private:

    auto
    poll_metrics(const std::error_code& ec) -> void;

private:

    // TODO: wip, possibility of redesign
    struct metrics_handle_t : public api::metrics_handle_base_t
    {
        using response_type = api::metrics_handle_base_t::response_type;

        metrics_handle_t(std::shared_ptr<metrics_retriever_t> parent) :
            parent{parent}
        {}

        auto
        on_data(const response_type& data) -> void override;

        auto
        on_error(const std::error_code&, const std::string& what) -> void override;

        std::shared_ptr<metrics_retriever_t> parent;
    };

    // TODO: wip, possibility of redesign
    struct metrics_pool_observer_t : public cocaine::service::node::pool_observer {

        metrics_pool_observer_t(metrics_retriever_t& p) :
            parent(p)
        {}

        auto
        spawned(const std::string&) -> void override
        {}

        auto
        despawned(const std::string& id) -> void override {
            parent.add_post_mortem(id);
        }

    private:
        metrics_retriever_t& parent;
    };

}; // metrics_retriever_t

template<typename Observers>
auto
metrics_retriever_t::make_and_ignite(
    context_t& ctx,
    const std::string& name,
    std::shared_ptr<api::isolate_t> isolate,
    const std::shared_ptr<engine_t>& parent_engine,
    asio::io_service& loop,
    synchronized<Observers>& observers) -> std::shared_ptr<metrics_retriever_t>
{
    if (!isolate) {
        throw error_t(cocaine::error::component_not_registered, "isolate daemon object wasn't provided");
    }

    // TODO: node service can be set with another name
    const auto node_config = ctx.config().component_group("services").get("node");

    if (node_config) {
        const auto args = node_config->args().as_object();

        const auto& should_start = args.at("isolate_metrics", false).as_bool();
        const auto& poll_interval = args.at("isolate_metrics_poll_period_s", conf::metrics_poll_interval_s).as_uint();

        if (!should_start) {
            throw error_t(cocaine::error::component_not_registered, "'isolate_metrics' wasn't set in config");
        }

        auto retriever = std::make_shared<metrics_retriever_t>(
            ctx,
            name,
            std::move(isolate),
            parent_engine,
            loop,
            poll_interval);

        observers->emplace_back(retriever->make_observer());
        retriever->ignite_poll();

        return retriever;
    }

    throw error_t(cocaine::error::component_not_found, "node server config section wasn't found");
}

}  // namespace node
}  // namespace service
}  // namespace detail
}  // namespace cocaine
