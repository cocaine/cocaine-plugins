#include <cocaine/unique_id.hpp>

#include "cocaine/service/node/slave/id.hpp"

namespace cocaine {
namespace service {
namespace node {
namespace slave {

id_t::id_t() : data(unique_id_t().string()) {}
id_t::id_t(std::string id) : data(std::move(id)) {}

auto id_t::id() const noexcept -> const std::string& {
    return data;
}

}  // namespace slave
}  // namespace node
}  // namespace service
}  // namespace cocaine
