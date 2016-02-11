#include "cocaine/service/metrics.hpp"

#include <cocaine/context.hpp>
#include <cocaine/traits/map.hpp>

#include <metrics/accumulator/sliding/window.hpp>
#include <metrics/counter.hpp>
#include <metrics/registry.hpp>
#include <metrics/tagged.hpp>
#include <metrics/timer.hpp>

namespace cocaine {
namespace service {

namespace ph = std::placeholders;

namespace {

template<typename T>
struct from_traits;

template<>
struct from_traits<std::map<std::string, std::string>> {
    static
    metrics::tagged_t
    from(std::map<std::string, std::string> tags) {
        const auto it = tags.find("name");
        if (it == tags.end()) {
            throw std::invalid_argument("tags must contain `name` tag");
        }

        return metrics::tagged_t(it->second, std::move(tags));
    }
};

}  // namespace

metrics_t::metrics_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args):
    category_type(context, asio, name, args),
    dispatch<io::metrics_tag>(name),
    log(context.log(name)),
    registry(context.metrics())
{
    on<io::metrics::counter_get>(std::bind(&metrics_t::on_counter_get, this, ph::_1));
    on<io::metrics::timer_get>(std::bind(&metrics_t::on_timer_get, this, ph::_1));
}

std::uint64_t
metrics_t::on_counter_get(tags_type tags) const {
    const auto tagged = from_traits<std::map<std::string, std::string>>::from(std::move(tags));
    const auto counter = registry.counter<std::int64_t>(tagged.name(), tagged.tags());

    return counter.get();
}

cocaine::result_of<io::metrics::timer_get>::type
metrics_t::on_timer_get(tags_type tags) const {
    const auto tagged = from_traits<std::map<std::string, std::string>>::from(std::move(tags));
    const auto metric = registry.timer(tagged.name(), tagged.tags());

    const auto snapshot = metric.snapshot();

    const auto result = std::make_tuple(
        metric.count(),
        metric.m01rate(),
        metric.m05rate(),
        metric.m15rate(),
        snapshot.median(),
        snapshot.p75(),
        snapshot.p90(),
        snapshot.p95(),
        snapshot.p98(),
        snapshot.p99()
    );

    return result;
}

}  // namespace service
}  // namespace cocaine
