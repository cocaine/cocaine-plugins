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

#include "stream.hpp"

#include "driver.hpp"

#include <cocaine/logging.hpp>

using namespace cocaine;
using namespace cocaine::driver;
using namespace cocaine::logging;

zmq_stream_t::zmq_stream_t(zmq_t& driver,
                           const std::shared_ptr<log_t>& log,
                           const route_t& route):
    m_driver(driver),
    m_log(log),
    m_route(route)
{ }

void
zmq_stream_t::push(const char * chunk,
                   size_t size)
{
    zmq::message_t message(size);

    std::memcpy(
        message.data(),
        chunk,
        size
    );
    
    m_driver.send(
        m_route.front(),
        message
    );
}

void
zmq_stream_t::error(error_code code,
                    const std::string& message)
{
    COCAINE_LOG_ERROR(
        m_log,
        "error while processing an event: [%d] %s",
        code,
        message
    );
}

void
zmq_stream_t::close() {
    // Pass.
}

