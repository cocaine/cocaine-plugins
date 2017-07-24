#include "cocaine/service/uniresis.hpp"

#include <asio/io_service.hpp>
#include <asio/ip/tcp.hpp>

#include <blackhole/logger.hpp>

#include <cocaine/api/unicorn.hpp>
#include <cocaine/context.hpp>
#include <cocaine/context/config.hpp>
#include <cocaine/dynamic.hpp>
#include <cocaine/executor/asio.hpp>
#include <cocaine/unicorn/value.hpp>
#include <cocaine/unique_id.hpp>

#include "cocaine/uniresis/error.hpp"

namespace cocaine {
namespace service {

using namespace asio::ip;

namespace {
namespace defaults {

const std::string locator_name = "locator";
const std::string unicorn_name = "core";
const std::string resources_path = "/resources";

} // namespace defaults
} // namespace

namespace {

namespace ph = std::placeholders;

auto resolve(const std::string& hostname) -> std::vector<tcp::endpoint> {
    std::vector<tcp::endpoint> endpoints;

    const tcp::resolver::query::flags flags =
        tcp::resolver::query::address_configured;

    asio::io_service loop;
    tcp::resolver::iterator begin = tcp::resolver(loop).resolve(
        tcp::resolver::query(hostname, "", flags)
    );

    std::transform(
        begin,
        tcp::resolver::iterator(),
        std::back_inserter(endpoints),
        std::bind(&tcp::resolver::iterator::value_type::endpoint, ph::_1)
    );

    return endpoints;
}

} // namespace

/// A task that will try to notify about resources information on the machine.
class uniresis_t::updater_t : public std::enable_shared_from_this<uniresis_t::updater_t> {
    std::string path;
    std::vector<tcp::endpoint> endpoints;
    uniresis::resources_t resources;
    std::shared_ptr<api::unicorn_t> unicorn;
    api::unicorn_scope_ptr scope;
    executor::owning_asio_t executor;
    asio::deadline_timer timer;
    std::shared_ptr<logging::logger_t> log;

public:
    updater_t(std::string path,
              std::vector<tcp::endpoint> endpoints,
              uniresis::resources_t resources,
              std::shared_ptr<api::unicorn_t> unicorn,
              std::shared_ptr<logging::logger_t> log) :
        path(std::move(path)),
        endpoints(std::move(endpoints)),
        resources(std::move(resources)),
        unicorn(std::move(unicorn)),
        scope(),
        executor(),
        timer(executor.asio()),
        log(std::move(log))
    {}

    auto
    notify() -> void {
        COCAINE_LOG_DEBUG(log, "schedule resource notification on `{}` ...", path);
        scope = unicorn->create(
            std::bind(&updater_t::on_create, shared_from_this(), ph::_1),
            path,
            make_value(),
            true,
            false
        );
    }

private:
    auto
    make_value() -> dynamic_t {
        dynamic_t::object_t result;

        std::vector<dynamic_t> endpoints;
        for (auto endpoint : this->endpoints) {
            endpoints.push_back({
                std::vector<dynamic_t>{{endpoint.address().to_string()}, {endpoint.port()}}
            });
        }
        result["endpoints"] = endpoints;

        result["resources"].as_object()["cpu"] = resources.cpu;
        result["resources"].as_object()["mem"] = resources.mem;

        return result;
    }

    auto
    notify_later() -> void {
        auto self = shared_from_this();

        COCAINE_LOG_DEBUG(log, "schedule resource notification after {} sec ...", 1);
        timer.expires_from_now(boost::posix_time::seconds(1));
        timer.async_wait([&, self](std::error_code ec) {
            if (ec) {
                return;
            }

            notify();
        });
    }

    auto
    on_create(std::future<bool> future) -> void {
        try {
            auto created = future.get();

            if (created) {
                COCAINE_LOG_INFO(log, "registered machine's resources on `{}` path", path);
            } else {
                COCAINE_LOG_ERROR(log, "failed to create `{}` node: already exists", path);
                notify_later();
            }
        } catch (const std::system_error& err) {
            COCAINE_LOG_ERROR(log, "failed to create `{}` node: {}", path, error::to_string(err));
            notify_later();
        } catch (const std::exception& err) {
            COCAINE_LOG_ERROR(log, "failed to create `{}` node: {}", path, err.what());
            notify_later();
        }
    }
};

uniresis_t::uniresis_t(context_t& context, asio::io_service& loop, const std::string& name, const dynamic_t& args) :
    api::service_t(context, loop, name, args),
    dispatch<io::uniresis_tag>(name),
    uuid(unique_id_t().string()),
    resources(),
    updater(nullptr),
    log(context.log("uniresis"))
{
    if (resources.cpu == 0) {
        throw std::system_error(uniresis::uniresis_errc::failed_calculate_cpu_count);
    }

    if (resources.mem == 0) {
        throw std::system_error(uniresis::uniresis_errc::failed_calculate_system_memory);
    }

    auto restrictions = args.as_object().at("restrictions", dynamic_t::empty_object).as_object();

    auto cpu_restricted = std::min(
        resources.cpu,
        static_cast<uint>(restrictions.at("cpu", resources.cpu).as_uint())
    );

    if (resources.cpu != cpu_restricted) {
        resources.cpu = cpu_restricted;
        COCAINE_LOG_INFO(log, "restricted available CPU count to {}", resources.cpu);
    }

    auto mem_restricted = std::min(
        resources.mem,
        static_cast<std::uint64_t>(restrictions.at("mem", resources.mem).as_uint())
    );

    if (resources.mem != mem_restricted) {
        resources.mem = mem_restricted;
        COCAINE_LOG_INFO(log, "restricted available system memory to {}", resources.mem);
    }

    auto prefix = args.as_object().at("prefix", defaults::resources_path).as_string();
    auto path = format("{}/{}", prefix, uuid);

    auto endpoints = resolve(context.config().network().hostname());
    auto unicorn = api::unicorn(context, args.as_object().at("unicorn", defaults::unicorn_name).as_string());
    updater = std::make_shared<updater_t>(
        std::move(path),
        std::move(endpoints),
        resources,
        std::move(unicorn),
        log
    );
    updater->notify();

    on<io::uniresis::cpu_count>([&] {
        return resources.cpu;
    });

    on<io::uniresis::memory_count>([&] {
        return resources.mem;
    });

    on<io::uniresis::uuid>([&] {
        return uuid;
    });
}

} // namespace service
} // namespace cocaine
