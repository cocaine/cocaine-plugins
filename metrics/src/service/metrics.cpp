#include "cocaine/service/metrics.hpp"

#include <cocaine/context/config.hpp>
#include <cocaine/format.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/traits/dynamic.hpp>
#include <cocaine/traits/map.hpp>

#include <cocaine/postgres/pool.hpp>

#include <blackhole/logger.hpp>

#include "metrics/extract.hpp"
#include "metrics/factory.hpp"
#include "metrics/filter/and.hpp"
#include "metrics/filter/contains.hpp"
#include "metrics/filter/ge.hpp"
#include "metrics/filter/eq.hpp"
#include "metrics/filter/or.hpp"
#include "metrics/visitor/dendroid.hpp"
#include "metrics/visitor/plain.hpp"

namespace cocaine {
namespace service {

namespace {

const std::array<std::tuple<std::string, metrics_t::type_t>, 2> types = {{
    std::make_tuple("json", metrics_t::type_t::json),
    std::make_tuple("plain", metrics_t::type_t::plain)
}};

}  // namespace

metrics_t::metrics_t(context_t& context,
                     asio::io_service& asio,
                     const std::string& _name,
                     const dynamic_t& args) :
    api::service_t(context, asio, _name, args),
    dispatch<io::metrics_tag>(_name),
    hub(context.metrics_hub()),
    senders(),
    registry(std::make_shared<metrics::registry_t>())
{
    registry->add(std::make_shared<metrics::tag_t>());
    registry->add(std::make_shared<metrics::name_t>());
    registry->add(std::make_shared<metrics::type_t>());
    registry->add(std::make_shared<metrics::const_t>());

    registry->add(std::make_shared<metrics::filter::eq_t>());
    registry->add(std::make_shared<metrics::filter::ge_t>());
    registry->add(std::make_shared<metrics::filter::or_t>());
    registry->add(std::make_shared<metrics::filter::and_t>());
    registry->add(std::make_shared<metrics::filter::contains_t>());

    auto sender_names = args.as_object().at("senders", dynamic_t::empty_array).as_array();

    // TODO: should we provide an opportunity to select output type to the user?
    // const auto out_type = args.as_object().at("output_type", "json").as_string();
    //
    // throw (fail) early in this::ctor on incorrect user provided type
    // make_type(out_type);

    const auto out_type = std::string("json");
    const auto filter_ast = args.as_object().at("filter_ast", dynamic_t::null);

    for(auto& sender_name: sender_names) {
        api::sender_t::data_provider_ptr provider(new api::sender_t::function_data_provider_t([=]() {
            return metrics(out_type, filter_ast);
        }));
        senders.push_back(api::sender(context, asio, sender_name.as_string(), std::move(provider)));
    }

    on<io::metrics::fetch>([&](const std::string& type, const dynamic_t& query) -> dynamic_t {
        return metrics(type, query);
    });
}

auto metrics_t::metrics(const std::string& type, const dynamic_t& query) const -> dynamic_t {
    const auto ty = make_type(type);
    const auto filter = make_filter(query);

    if (ty == type_t::json) {
        return construct_dendroid(filter);
    } else {
        return construct_plain(filter);
    }
}

auto
metrics_t::make_type(const std::string& type) const -> type_t {
    if (type.empty()) {
        return type_t::plain;
    }

    const auto it = std::find_if(
        std::begin(types),
        std::end(types),
        [&](const std::tuple<std::string, metrics_t::type_t>& ty) -> bool {
            return std::get<0>(ty) == type;
        }
    );

    if (it == std::end(types)) {
        throw cocaine::error_t("unknown output type");
    }

    return std::get<1>(*it);
}

auto
metrics_t::make_filter(const dynamic_t& query) const -> libmetrics::query_t {
    if (query.is_null()) {
        return [](const libmetrics::tagged_t&) -> bool {
            return true;
        };
    } else {
        return registry->make_filter(query);
    }
}

auto
metrics_t::construct_plain(const libmetrics::query_t& filter) const -> dynamic_t {
    dynamic_t::object_t out;
    for (const auto& metric : hub.select(filter)) {
        metrics::plain_t visitor(metric->name(), out);
        metric->apply(visitor);
    }

    return out;
}

auto
metrics_t::construct_dendroid(const libmetrics::query_t& filter) const -> dynamic_t {
    dynamic_t::object_t out;
    for (const auto& metric : hub.select(filter)) {
        metrics::dendroid_t visitor(metric->name(), out);
        metric->apply(visitor);
    }

    return out;
}

}  // namespace service
}  // namespace cocaine
