#pragma once

#include <cocaine/api/service.hpp>
#include <cocaine/rpc/dispatch.hpp>

#include <map>
#include <string>

#include "cocaine/idl/metrics.hpp"

namespace metrics {

class registry_t;

}  // namespace metrics

namespace cocaine {
namespace service {

class metrics_t:
    public api::service_t,
    public dispatch<io::metrics_tag>
{
    const std::unique_ptr<logging::log_t> log;

    metrics::registry_t& registry;

    typedef std::map<std::string, std::string> tags_type;

public:
    metrics_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args);

    const io::basic_dispatch_t&
    prototype() const {
        return *this;
    }

private:
    /// Returns a value of the counter that matches to a tags equivalent to `tags`.
    std::uint64_t
    on_counter_get(tags_type tags) const;

    /// Returns the timer statistics of the timer that matches to the given `tags`.
    cocaine::result_of<io::metrics::timer_get>::type
    on_timer_get(tags_type tags) const;
};

}  // namespace service
}  // namespace cocaine
