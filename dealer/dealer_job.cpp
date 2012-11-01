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

#include "dealer_job.hpp"

using namespace cocaine::engine;
using namespace cocaine::driver;

namespace cocaine {

namespace driver {
    struct dealer_tag;

    struct ack {
        typedef dealer_tag tag;

        typedef boost::tuple<
            const std::string&
        > tuple_type;
    };

    struct chunk {
        typedef dealer_tag tag;

        typedef boost::tuple<
            const std::string&,
            zmq::message_t&
        > tuple_type;
    };

    struct error {
        typedef dealer_tag tag;

        typedef boost::tuple<
            const std::string&,
            int,
            const std::string&
        > tuple_type;
    };

    struct choke {
        typedef dealer_tag tag;
        
        typedef boost::tuple<
            const std::string&
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

dealer_job_t::dealer_job_t(const std::string& event, 
                           const policy_t& policy,
                           io::channel<io::policies::unique>& channel,
                           const route_t& route,
                           const std::string& tag):
    job_t(event, policy),
    m_channel(channel),
    m_route(route),
    m_tag(tag)
{
    io::message<ack> message(m_tag);
    send(m_route.front(), message);
}

void
dealer_job_t::react(const events::chunk& event) {
    io::message<chunk> message(m_tag, event.message);
    send(m_route.front(), message);
}

void
dealer_job_t::react(const events::error& event) {
    io::message<error> message(m_tag, event.code, event.message);
    send(m_route.front(), message);
}

void
dealer_job_t::react(const events::choke& event) {
    io::message<choke> message(m_tag);
    send(m_route.front(), message);
}
