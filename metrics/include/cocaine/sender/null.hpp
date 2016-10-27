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

#pragma once

#include "cocaine/api/sender.hpp"

namespace cocaine {
namespace sender {

class null_sender_t : public api::sender_t {
public:
    null_sender_t(context_t& context,
                  asio::io_service& io_service,
                  const std::string& name,
                  data_provider_ptr data_provider,
                  const dynamic_t& args
    ) :
        api::sender_t(context, io_service, name, std::move(data_provider), args)
    {}
};

}
}
