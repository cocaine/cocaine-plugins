#pragma once

#include <boost/filesystem/operations.hpp>
#include <boost/lexical_cast.hpp>

#include <cocaine/rpc/actor.hpp>

namespace cocaine {
namespace service {
namespace node {

class unix_actor_t : public actor_base<asio::local::stream_protocol> {
    endpoint_type endpoint;
    std::unique_ptr<logging::logger_t> log;

public:
    unix_actor_t(context_t& context, std::string endpoint, std::unique_ptr<io::basic_dispatch_t> prototype) :
        actor_base(context, std::move(prototype)),
        endpoint(std::move(endpoint)),
        log(context.log("core/asio", {{"service", this->prototype()->name()}}))
    {}

    virtual ~unix_actor_t() {}

    auto
    endpoints() const -> std::vector<endpoint_type> override {
        return {{endpoint}};
    }

protected:
    auto
    make_endpoint() const -> endpoint_type override {
        return endpoint;
    }

    auto
    on_terminate() -> void override {
        const auto endpoint = boost::lexical_cast<std::string>(this->endpoint);

        try {
            boost::filesystem::remove(endpoint);
        } catch (const std::exception& err) {
            COCAINE_LOG_WARNING(log, "unable to clean local endpoint '{}': {}", endpoint, err.what());
        }
    }
};

}
} // namespace service
} // namespace cocaine
