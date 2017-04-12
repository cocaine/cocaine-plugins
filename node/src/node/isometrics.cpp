#include <tuple>
#include <cassert>
#include <unordered_map>

#include "isometrics.hpp"

#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/numeric.hpp>

// #define ISOMETRICS_DEBUG
#undef ISOMETRICS_DEBUG

// TODO: deprecated
#ifdef ISOMETRICS_DEBUG
#define dbg(msg) std::cerr << msg << '\n'
#define DBG_DUMP_UUIDS(os, logo, container) aux::dump_uuids(os, logo, container)
#else
#define dbg(msg)
#define DBG_DUMP_UUIDS(os, logo, container)
#endif

namespace cocaine {
namespace detail {
namespace service {
namespace node {

namespace ph = std::placeholders;

namespace conf {
    constexpr auto purgatory_queue_bound = 8 * 1024;
    constexpr auto show_errors_limit = 3;

    // Number of poll iterations without worker metrics updates, used to signal
    // (post warning to log, etc,) the cases when isolate is active,
    // but for some reason `forget` to send metrics for active worker without
    // signaling any error.
    constexpr auto missed_updates_times = 10;

    const std::vector<std::pair<std::string, counter_type_t>> counter_metrics_names =
    {
        // yet abstract cpu load measurement
        {"cpu", counter_type_t::instant},

        // memory usage (in bytes)
        {"vms", counter_type_t::instant},
        {"rss", counter_type_t::instant},

        // running times
        {"uptime",    counter_type_t::aggregate},
        {"user_time", counter_type_t::aggregate},
        {"sys_time",  counter_type_t::aggregate},

        // disk io (in bytes)
        {"ioread",  counter_type_t::aggregate},
        {"iowrite", counter_type_t::aggregate},

        // TODO: network stats

    };
}

namespace detail {

    template<typename Val>
    auto
    uuid_value(const Val& v) -> std::string;

    template<>
    auto
    uuid_value(const std::string& v) -> std::string {
        return v;
    }

    template<>
    auto
    uuid_value(const dynamic_t& arr) -> std::string {
        return arr.as_string();
    }

    template<typename Container>
    auto
    dump_uuids(std::ostream& os, const std::string& logo, const Container& arr) -> void {
        os << logo << " size " << arr.size() << '\n';
        for(const auto& uuid : arr) {
            os << "\tuuid: " << uuid_value(uuid) << '\n';
        }
    }

    template<typename T>
    auto
    make_counter(context_t& ctx, const std::string& name)
        -> metrics::shared_metric<std::atomic<T>>
    {
        return ctx.metrics_hub().counter<std::uint64_t>(name);
    }

    auto
    make_uint_counter(context_t& ctx, const std::string& pfx, const std::string& name)
        -> metrics::shared_metric<std::atomic<std::uint64_t>>
    {
        return make_counter<std::uint64_t>(ctx, cocaine::format("{}.{}", pfx, name));
    }
}

auto
metrics_aggregate_proxy_t::operator+(const worker_metrics_t& worker_metrics) -> metrics_aggregate_proxy_t&
{
    dbg("worker_metrics_t::operator+() summing to proxy");

    for(const auto& worker_record : worker_metrics.common_counters) {
        const auto& name = worker_record.first;
        const auto& metric = worker_record.second;

        auto& self_record = this->common_counters[name];

        self_record.values += metric.value->load();
        self_record.deltas += metric.delta;
    }

    return *this;
}

auto
operator+(worker_metrics_t& src, metrics_aggregate_proxy_t& proxy) -> metrics_aggregate_proxy_t&
{
    return proxy + src;
}

auto
worker_metrics_t::assign(metrics_aggregate_proxy_t&& proxy) -> void {

    dbg("worker_metrics_t::operator=() moving from proxy");

    if (proxy.common_counters.empty()) {

        // no active workers, zero out some metrics values
        for(const auto& init_metrics : conf::counter_metrics_names) {
            const auto& name = init_metrics.first;
            const auto& type = init_metrics.second;

            dbg("preserved " << name << " : " << static_cast<int>(type));

            auto self_it = this->common_counters.find(name);
            if (self_it != std::end(this->common_counters) && type == counter_type_t::instant) {
                    self_it->second.value->store(0);
            }
        }

    } else {

        for(const auto& proxy_metrics : proxy.common_counters) {
            const auto& name = proxy_metrics.first;
            const auto& proxy_record = proxy_metrics.second;

            auto self_it = this->common_counters.find(name);
            if (self_it != std::end(this->common_counters)) {
                auto& self_record = self_it->second;

                switch (self_record.type) {
                    case counter_type_t::aggregate:
                        self_record.value->fetch_add(proxy_record.deltas);
                        break;
                    case counter_type_t::instant:
                        self_record.value->store(proxy_record.values);
                        break;
                }
            }

        } // for metrics in proxy
    }
}

// TODO: error messages processing
struct response_processor_t {
    using stats_table_type = metrics_retriever_t::stats_table_type;
    using faded_update_mapping_type = std::unordered_map<std::string, std::chrono::seconds>;

    struct error_t {
        long code;
        std::string message;
    };

    response_processor_t(const std::string app_name) :
        app_name(app_name)
    {}

    auto
    operator()(context_t& ctx, const dynamic_t& response, stats_table_type& stats_table,
        const std::chrono::seconds& faded_timeout) -> size_t
    {
        return process(ctx, response, stats_table, faded_timeout);
    }

    auto
    process(context_t& ctx, const dynamic_t& response, stats_table_type& stats_table,
       const std::chrono::seconds& faded_timeout) -> size_t
    {
        const auto apps = response.as_object();

        auto uuids_processed = size_t{};

        for(const auto& app : apps) {

            // TODO: restore app_name assertion check
            //
            // strictly, we should have one app at all within current protocol
            // if (app_name != app.first) {
            //     dbg("JSON app is " << app.first);
            //     continue;
            // }

            const auto uuids = app.second.as_object();
            const auto now = worker_metrics_t::clock_type::now();

            for(const auto& item : uuids) {
                const auto& uuid = item.first;
                const auto& metrics = item.second.as_object();

                if (uuid.empty()) {
                    dbg("[response] 'uuid' value is empty, ignoring");
                    continue;
                }

                if (uuid == std::string("error")) {
                    parse_error_record(metrics);
                    continue;
                }

                auto stat_it = stats_table.find(uuid);
                if (stat_it == std::end(stats_table)) {
                    // If isolate daemon sends us uuid we don't know about,
                    // add it to the stats (monitoring) table. If it was send by error, it
                    // would be removed from request list and from stats table
                    // on next poll iteration preparation.
                    dbg("[response] inserting new metrics record with uuid" << uuid);
                    std::tie(stat_it, std::ignore) = stats_table.emplace(uuid, worker_metrics_t{ctx, app_name, uuid});
                }

                auto common_it = metrics.find("common");
                if (common_it == std::end(metrics)) {
                    dbg("[response] no `common` section");
                    continue;
                }

                const auto& updated_count = this->fill_metrics(common_it->second.as_object(), stat_it->second.common_counters);

                if (updated_count) {
                    stat_it->second.update_stamp = now;
                } else {
                    const auto& update_span = now - stat_it->second.update_stamp;
                    if (update_span > faded_timeout) {
                        faded.emplace(uuid, std::chrono::duration_cast<std::chrono::seconds>(update_span));
                    }
                }
            }
        }

        return uuids_processed;
    }

    auto
    errors_count() const -> std::size_t {
        return isolate_errors.size();
    }

    auto
    has_errors() const -> bool {
        return !isolate_errors.empty();
    }

    auto
    errors() -> const std::vector<error_t>& {
        return isolate_errors;
    }

    auto
    has_faded() const -> bool {
        return !faded.empty();
    }

    auto
    faded_updates() -> const faded_update_mapping_type& {
        return faded;
    }

private:

    auto
    parse_error_record(const dynamic_t::object_t& metrics) -> void {
        const auto& error_message = metrics.at("error.message", "none").as_string();
        const auto& error_code    = metrics.at("error.code", 0).as_int();

        isolate_errors.emplace_back(error_t{error_code, error_message});

        dbg("[response] got an error: " << error_message << " code: " << error_code);
    }

    auto
    fill_metrics(const dynamic_t::object_t& metrics, worker_metrics_t::counters_table_type& result) -> size_t {

        auto updated = size_t{};

        if (metrics.count("error.message")) {
            parse_error_record(metrics);
            return updated;
        }

        for(const auto& metric : metrics) {
            const auto& name = metric.first;

            dbg("[response] metrics name: " << name);

            std::string nm;
            // type not checked (for now)
            std::tie(nm, std::ignore) = decay_metric_name(name);

            if (nm.empty()) {
                // protocol error: ingore silently
                continue;
            }

            try {
                auto r = result.find(nm);
                if (r != std::end(result)) {
                    // we are interested in this metrics, its name was registered
                    dbg("[response] found metrics record for " << nm << ", updating...");

                    auto& record = r->second;
                    const auto& incoming_value = metric.second.as_uint();

                    if (record.type == counter_type_t::aggregate) {
                        const auto& current = record.value->load();
                        if (incoming_value >= current) {
                            record.delta = incoming_value - current;
                        } // else: client misbehavior, shouldn't try to store negative deltas
                    }

                    record.value->store(incoming_value);
                    ++updated;
                }

            } catch (const boost::bad_get& e) {
                dbg("[response] parsing exception: " << e.what());
                continue;
            }
        } // for each metric

        dbg("[response] fill_metrics done");
        return updated;
    }

private:

    auto
    decay_metric_name(const std::string& name, const char sep='.') -> std::tuple<std::string, std::string> {
        // cut off type from name
        const auto pos = name.find(sep);

        if (pos == std::string::npos) {
            return std::make_tuple(name, "");
        }

        const auto nm = name.substr(0, pos);
        const auto tp = name.substr(pos + 1);

        return std::make_tuple(nm, tp);
    }

    std::string app_name;
    std::vector<error_t> isolate_errors;

    // Table of workers uuids (and their update time stamps) which wasn't
    // updated for poll_interval * missed_updates_times period of time.
    // Used to signal (post to log) a warning.
    faded_update_mapping_type faded;
};

worker_metrics_t::worker_metrics_t(context_t& ctx, const std::string& name_prefix) :
    update_stamp(clock_type::now())
{
    for(const auto& metrics_init : conf::counter_metrics_names) {
        const auto& name = metrics_init.first;
        const auto& type = metrics_init.second;

        common_counters.emplace(name,
            counter_metric_t{
                type,
                detail::make_uint_counter(ctx, name_prefix, name),
                0 });
    }
}

worker_metrics_t::worker_metrics_t(context_t& ctx, const std::string& app_name, const std::string& id) :
    worker_metrics_t(ctx, cocaine::format("{}.isolate.{}", app_name, id))
{}

metrics_retriever_t::self_metrics_t::self_metrics_t(context_t& ctx, const std::string& pfx) :
   uuid_requested{detail::make_uint_counter(ctx, pfx, "uuid_requested")},
   uuid_recieved{detail::make_uint_counter(ctx, pfx, "uuid_recieved")},
   requests_send{detail::make_uint_counter(ctx, pfx, "requests")},
   responses_received{detail::make_uint_counter(ctx, pfx, "responses")},
   receive_errors{detail::make_uint_counter(ctx, pfx, "recieve.errors")},
   posmortem_queue_size{detail::make_uint_counter(ctx, pfx, "postmortem.queue.size")}
{}

metrics_retriever_t::metrics_retriever_t(
    context_t& ctx,
    const std::string& name, // app name
    std::shared_ptr<api::isolate_t> isolate,
    synchronized<engine_t::pool_type>& pool,
    asio::io_service& loop,
    const std::uint64_t poll_interval) :
        context(ctx),
        metrics_poll_timer(loop),
        isolate(std::move(isolate)),
        pool(pool),
        log(ctx.log(format("{}/workers_metrics", name))),
        self_metrics(ctx, "node.isolate.poll.metrics"),
        app_name(name),
        poll_interval(poll_interval),
        app_aggregate_metrics(ctx, cocaine::format("{}.isolate", name))
{
    COCAINE_LOG_INFO(log, "worker metrics retriever has been initialized");
}

auto
metrics_retriever_t::ignite_poll() -> void {
    metrics_poll_timer.expires_from_now(poll_interval);
    metrics_poll_timer.async_wait(std::bind(&metrics_retriever_t::poll_metrics, shared_from_this(), ph::_1));
}

auto
metrics_retriever_t::add_post_mortem(const std::string& id) -> void {
    if (purgatory->size() >= conf::purgatory_queue_bound) {
        // on high despawn rates stat of some unlucky workers will be lost
        COCAINE_LOG_INFO(log, "worker metrics retriever: dead worker stat will be discarded for {}", id);
        return;
    }

    purgatory.apply([&](purgatory_pot_type& pot) {
        pot.emplace(id);
        self_metrics.posmortem_queue_size->store(pot.size());
    });
}

auto
metrics_retriever_t::poll_metrics(const std::error_code& ec) -> void {
    using boost::adaptors::map_keys;

    if (ec) {
        // cancelled
        COCAINE_LOG_WARNING(log, "workers metrics polling was cancelled");
        return;
    }

    if (!isolate) {
        ignite_poll();
        return;
    }

    auto alive_uuids = pool.apply([] (const engine_t::pool_type& pool) {
        std::vector<std::string> alive;
        alive.reserve(pool.size());

        boost::copy(pool | map_keys, std::back_inserter(alive));
        return alive;
    });

    DBG_DUMP_UUIDS(std::cerr, "metrics.pool", alive_uuids);

#ifdef ISOMETRICS_DEBUG
    purgatory.apply([&](const purgatory_pot_type& pot) {
        DBG_DUMP_UUIDS(std::cerr, "purgatory", pot);
    });
#endif

    dynamic_t::array_t query_array;
    query_array.reserve(alive_uuids.size() + purgatory->size());

    // Note: it is promised by devs that active list should be quite
    // small ~ hundreds of workers, so it seems reasonable to pay a little for
    // sorting here, but code should be redisigned if average alive count
    // will increase significantly
    boost::sort(alive_uuids);

    purgatory.apply([&](purgatory_pot_type& pot) {
        boost::set_union(alive_uuids, pot, std::back_inserter(query_array));
        pot.clear();
        self_metrics.posmortem_queue_size->store(0);
    });

    DBG_DUMP_UUIDS(std::cerr, "query array", query_array);

    dynamic_t::object_t query;
    query["uuids"] = query_array;
    isolate->metrics(query, std::make_shared<metrics_handle_t>(shared_from_this()));

    // At this point query is posted and we have gathered uuids of available
    // (alived, pooled) workers and dead recently workers, so we can clear
    // stat_table out of garbage `neither alive nor dead` uuids
    stats_table_type preserved_metrics;
    preserved_metrics.reserve(metrics->size());

    metrics.apply([&](stats_table_type& table) {
        for(const auto to_preserve : query_array) {
            const auto it = table.find(to_preserve.as_string());

            if (it != std::end(table)) {
                preserved_metrics.emplace(std::move(*it));
            }
        }

        table.clear();
        table.swap(preserved_metrics);
    });

    // Update self stat
    self_metrics.uuid_requested->fetch_add(query_array.size());
    self_metrics.requests_send->fetch_add(1);

    ignite_poll();
}

auto
metrics_retriever_t::make_observer() -> std::shared_ptr<pool_observer> {
    return std::make_shared<metrics_pool_observer_t>(*this);
}

//// metrics_handle_t //////////////////////////////////////////////////////////

auto
metrics_retriever_t::metrics_handle_t::on_data(const dynamic_t& data) -> void {
    using namespace boost::adaptors;

    assert(parent);

    dbg("metrics_handle_t:::on_data");
    dbg("[response] get json: " << boost::lexical_cast<std::string>(data) << '\n');

    COCAINE_LOG_DEBUG(parent->log, "processing isolation metrics response");

    const auto faded_timeout = std::chrono::seconds(parent->poll_interval.total_seconds() * conf::missed_updates_times);
    response_processor_t processor(parent->app_name);

    // should not harm performance, as this handler would be called from same
    // poll loop, within same thread on each poll iteration
    const auto processed_count = parent->metrics.apply([&](metrics_retriever_t::stats_table_type& table) {

        // fill workers current `slice` state of metrics table
        const auto processed_count = processor(parent->context, data, table, faded_timeout);

        // update application-wide aggregate of metrics
        parent->app_aggregate_metrics.assign(
            boost::accumulate( table | map_values, metrics_aggregate_proxy_t()));

        return processed_count;
    });

    parent->self_metrics.uuid_recieved->fetch_add(processed_count);
    parent->self_metrics.responses_received->fetch_add(1);

    if (processor.has_errors()) {
        const auto& errors = processor.errors();
        auto break_counter = int{};

        for(const auto& e : errors) {
            if (++break_counter > conf::show_errors_limit) {
                break;
            }
            COCAINE_LOG_DEBUG(parent->log, "isolation metrics got an error {} {}", e.code, e.message);
        }
    }

    for(const auto& faded : processor.faded_updates()) {
        const auto& uuid = faded.first;
        const auto& duration = faded.second;

        COCAINE_LOG_WARNING(parent->log, "no isolate metrics for active worker {} for {} second(s)", uuid, duration.count());
    }
}

auto
metrics_retriever_t::metrics_handle_t::on_error(const std::error_code& error, const std::string& what) -> void {
    assert(parent);
    dbg("metrics_handle_t::on_error: " << what);

    if (error == error::not_connected) {
        return;
    }

    // TODO: start crying?
    COCAINE_LOG_WARNING(parent->log, "worker metrics recieve error {}:{}", error, what);
    parent->self_metrics.receive_errors->fetch_add(1);
}

} // namespace node
} // namespace service
} // namespace detail
} // namespace cocaine
