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

using namespace cocaine;
using namespace cocaine::driver;

namespace cocaine { namespace io {
    namespace tags {
        struct dealer_tag;
    }

    namespace dealer {
        struct ack {
            typedef tags::dealer_tag tag;

            typedef boost::mpl::list<
                std::string
            > tuple_type;
        };

        struct chunk {
            typedef tags::dealer_tag tag;

            typedef boost::mpl::list<
                std::string,
                std::string
            > tuple_type;
        };

        struct error {
            typedef tags::dealer_tag tag;

            typedef boost::mpl::list<
                std::string,
                int,
                std::string
            > tuple_type;
        };

        struct choke {
            typedef tags::dealer_tag tag;

            typedef boost::mpl::list<
                std::string
            > tuple_type;
        };
    }

    template<>
    struct protocol<tags::dealer_tag> {
        typedef boost::mpl::list<
            dealer::ack,
            dealer::chunk,
            dealer::error,
            dealer::choke
        >::type type;
    };
}} // namespace cocaine::io

dealer_stream_t::dealer_stream_t(dealer_t& driver,
                                 const route_t& route,
                                 const std::string& tag):
    m_driver(driver),
    m_route(route),
    m_tag(tag)
{
    m_driver.send<io::dealer::ack>(
        m_route.front(),
        m_tag
    );
}

void
dealer_stream_t::push(const char * chunk,
                      size_t size)
{
    m_driver.send<io::dealer::chunk>(
        m_route.front(),
        m_tag,
        std::string(chunk, size)
    );
}

void
dealer_stream_t::error(error_code code,
                       const std::string& message)
{
    m_driver.send<io::dealer::error>(
        m_route.front(),
        m_tag,
        static_cast<int>(code),
        message
    );

    close();
}

void
dealer_stream_t::close() {
    m_driver.send<io::dealer::choke>(
        m_route.front(),
        m_tag
    );
}
