/*
    Copyright (c) 2016+ Anton Matveenko <antmat@me.com>
    Copyright (c) 2016+ Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "cocaine/api/peer/pool.hpp"
#include "cocaine/context.hpp"
#include "cocaine/context/config.hpp"
#include "cocaine/repository/peer/pool.hpp"

#include <boost/optional/optional.hpp>

namespace cocaine {
namespace api {
namespace peer {

// Sender
auto pool(context_t& context,
          asio::io_service& io_service,
          const std::string& pool_name,
          const std::string& service_name) -> pool_ptr
{
    auto pool = context.config().component_group("vicodyn_pools").get(pool_name);
    if (!pool) {
        throw error_t(std::errc::argument_out_of_domain, "vicodyn pool component with name '{}' not found", pool_name);
    }
    return context.repository().get<pool_t>(pool->type(), context, io_service, service_name, pool->args());
}

} // namespace peer
} // namespace api
} // namespace cocaine
