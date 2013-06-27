/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_ZEROMQ_SERVER_STREAM_HPP
#define COCAINE_ZEROMQ_SERVER_STREAM_HPP

#include <cocaine/api/stream.hpp>

namespace cocaine { namespace driver {

typedef std::vector<
    std::string
> route_t;

class zmq_t;

struct zmq_stream_t:
    public api::stream_t
{
    zmq_stream_t(zmq_t& driver, const std::shared_ptr<logging::log_t>& log, const route_t& route);

    virtual
    void
    push(const char * chunk, size_t size);

    virtual
    void
    error(error_code code, const std::string& message);

    virtual
    void
    close();

private:
    zmq_t& m_driver;
    std::shared_ptr<logging::log_t> m_log;

    // NOTE: Even though all parts of the routing information is available
    // only the first part will be used for responding.
    const route_t m_route;
};

}}

#endif
