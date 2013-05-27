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

#ifndef COCAINE_DEALER_STREAM_HPP
#define COCAINE_DEALER_STREAM_HPP

#include "io.hpp"

#include <cocaine/api/stream.hpp>

namespace cocaine { namespace driver {

typedef std::vector<
    std::string
> route_t;

class dealer_t;

struct dealer_stream_t:
    public api::stream_t
{
    dealer_stream_t(dealer_t& driver,
                    const route_t& route,
                    const std::string& tag);

    virtual
    void
    write(const char * chunk,
         size_t size);

    virtual
    void
    error(int code,
          const std::string& message);

    virtual
    void
    close();

private:
    dealer_t& m_driver;

    const route_t m_route;
    const std::string m_tag;
};

}}

#endif
