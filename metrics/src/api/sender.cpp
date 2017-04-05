/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/api/sender.hpp"
#include "cocaine/context.hpp"
#include "cocaine/context/config.hpp"
#include "cocaine/repository/sender.hpp"

#include <boost/optional/optional.hpp>

namespace cocaine { namespace api {

// Sender
auto sender(context_t& context,
       asio::io_service& io_service,
       const std::string& name,
       sender_t::data_provider_ptr data_provider) -> sender_ptr
{
    auto sender = context.config().component_group("senders").get(name);
    if(!sender) {
        throw error_t(std::errc::argument_out_of_domain, "sender component with name '{}' not found", name);
    }
    return context.repository().get<sender_t>(sender->type(),
                                              context,
                                              io_service,
                                              name,
                                              std::move(data_provider),
                                              sender->args());
}


}} // namespace cocaine::api
