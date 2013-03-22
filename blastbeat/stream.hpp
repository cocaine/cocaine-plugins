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

#ifndef COCAINE_BLASTBEAT_STREAM_HPP
#define COCAINE_BLASTBEAT_STREAM_HPP

#include <cocaine/api/stream.hpp>

namespace cocaine { namespace driver {

class blastbeat_t;

struct blastbeat_stream_t:
    public api::stream_t
{
    blastbeat_stream_t(blastbeat_t& driver,
                       const std::string& sid);

    virtual
    void
    write(const char * chunk,
          size_t size);

    virtual
    void
    error(error_code code,
          const std::string& message);

    virtual
    void
    close();

private:
    blastbeat_t& m_driver;

    // Blastbeat session ID for this stream.
    const std::string m_sid;

    // Indicates that headers are already away.
    bool m_body;
};

}}

#endif
