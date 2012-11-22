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

using namespace cocaine;
using namespace cocaine::driver;

namespace cocaine {

namespace driver {
    struct ack {
        typedef dealer_tag tag;

        typedef boost::mpl::list<
            std::string
        > tuple_type;
    };

    struct chunk {
        typedef dealer_tag tag;

        typedef boost::mpl::list<
            std::string,
            std::string
        > tuple_type;
    };

    struct error {
        typedef dealer_tag tag;

        typedef boost::mpl::list<
            std::string,
            int,
            std::string
        > tuple_type;
    };

    struct choke {
        typedef dealer_tag tag;
        
        typedef boost::mpl::list<
            std::string
        > tuple_type;
    };
}

namespace io {
    template<>
    struct dispatch<dealer_tag> {
        typedef boost::mpl::list<
            driver::ack,
            driver::chunk,
            driver::error,
            driver::choke
        >::type category;
    };
}

} // namespace cocaine

dealer_stream_t::dealer_stream_t(rpc_channel_t& channel,
                                 const route_t& route,
                                 const std::string& tag):
    m_channel(channel),
    m_route(route),
    m_tag(tag)
{
    send<driver::ack>(
        m_route.front(),
        m_tag
    );
}

void
dealer_stream_t::push(const char * chunk,
                      size_t size)
{
    send<driver::chunk>(
        m_route.front(),
        m_tag,
        std::string(chunk, size)
    );
}

void
dealer_stream_t::error(error_code code,
                       const std::string& message)
{
    send<driver::error>(
        m_route.front(),
        m_tag,
        static_cast<int>(code),
        message
    );

    close();
}

void
dealer_stream_t::close() {
    send<driver::choke>(
        m_route.front(),
        m_tag
    );
}
