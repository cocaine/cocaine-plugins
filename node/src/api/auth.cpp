#include "cocaine/api/auth.hpp"

#include <cocaine/context.hpp>
#include <cocaine/context/config.hpp>
#include <cocaine/repository.hpp>

#include <boost/optional/optional.hpp>

#include "cocaine/repository/auth.hpp"

namespace cocaine {
namespace api {

namespace {

const char COMPONENT_NAME[] = "authorizations";

}  // namespace

auto auth(context_t& context, const std::string& name) -> std::shared_ptr<auth_t> {
    if (auto config = context.config().component_group(COMPONENT_NAME).get(name)) {
        return context.repository().get<auth_t>(config->type(), context, name, config->args());
    } else {
        throw std::system_error(std::make_error_code(std::errc::argument_out_of_domain), name);
    }
}

}  // namespace api
}  // namespace cocaine
