#include "cocaine/api/postgres/pool.hpp"

#include <cocaine/context.hpp>
#include <cocaine/context/config.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/repository.hpp>
#include <cocaine/repository/postgres/pool.hpp>

#include <boost/optional/optional.hpp>

namespace cocaine {
namespace api {
namespace postgres {

pool_ptr pool(context_t& context, const std::string& name) {
    auto pool = context.config().component_group("postgres").get(name);
    if(!pool) {
        throw error_t(std::errc::argument_out_of_domain, "postgres component with name '{}' not found", name);
    }
    return context.repository().get<pool_t>("postgres", context, name, pool->args());
}

} // namespace postgres
} // namespace api
} // namesapce cocaine
