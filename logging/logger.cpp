/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#include "logger.hpp"

using namespace cocaine;
using namespace cocaine::logger;

remote_t::remote_t(context_t& context,
                   const std::string& name,
                   const Json::Value& args):
    category_type(context, name, args),
    m_client(context, "logging")
{ }

void
remote_t::emit(logging::priorities priority,
               const std::string& source,
               const std::string& message)
{
    m_client.send<io::service::emit>(
        static_cast<int>(priority),
        source,
        message
    );
}
