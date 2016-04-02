#pragma once

#include "cocaine/api/isolate.hpp"
#include "cocaine/service/node/forwards.hpp"

#include <cocaine/forwards.hpp>

namespace cocaine {
namespace detail {
namespace service {
namespace node {
namespace slave {

class spawn_handle_t:
    public api::spawn_handle_base_t
{
public:
    spawn_handle_t(std::unique_ptr<cocaine::logging::logger_t> log,
                   std::weak_ptr<machine_t> slave,
                   std::shared_ptr<state::spawn_t> spawning);

    virtual
    void
    on_terminate(const std::error_code&, const std::string& msg);

    virtual
    void
    on_ready();

    virtual
    void
    on_data(const std::string& data);

private:
    std::unique_ptr<cocaine::logging::logger_t> log;
    std::weak_ptr<machine_t> slave;
    std::shared_ptr<state::spawn_t> spawning;
    std::chrono::high_resolution_clock::time_point start;
};

} // namespace slave
} // namespace node
} // namespace service
} // namespace detail
} // namespace cocaine
