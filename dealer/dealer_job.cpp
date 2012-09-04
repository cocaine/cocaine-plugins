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

struct dealer_tag;

struct acknowledgement { typedef dealer_tag tag; };
struct chunk           { typedef dealer_tag tag; };
struct error           { typedef dealer_tag tag; };
struct choke           { typedef dealer_tag tag; };

typedef boost::mpl::list<
    acknowledgement, chunk, error, choke
>::type dealer_category;

namespace cocaine { namespace io {

template<>
struct dispatch<dealer_tag> {
    typedef dealer_category category;
};

template<>
struct command<acknowledgement>:
    public boost::tuple<const std::string&>
{
    typedef boost::tuple<const std::string&> tuple_type;

    command(const std::string& tag):
        tuple_type(tag)
    { }
};

template<>
struct command<chunk>:
    public boost::tuple<const std::string&, zmq::message_t&>
{
    typedef boost::tuple<const std::string&, zmq::message_t&> tuple_type;

    command(const std::string& tag, zmq::message_t& message_):
        tuple_type(tag, message)
    {
        message.move(&message_);
    }

private:
    zmq::message_t message;
};

template<>
struct command<error>:
    public boost::tuple<const std::string&, int, const std::string&>
{
    typedef boost::tuple<const std::string&, int, const std::string&> tuple_type;

    command(const std::string& tag, int code, const std::string& message):
        tuple_type(tag, code, message)
    { }
};

template<>
struct command<choke>:
    public boost::tuple<const std::string&>
{
    typedef boost::tuple<const std::string&> tuple_type;

    command(const std::string& tag):
        tuple_type(tag)
    { }
};

}}

dealer_job_t::dealer_job_t(const std::string& event, 
                           const std::string& request,
                           const policy_t& policy,
                           io::channel_t& channel,
                           const route_t& route,
                           const std::string& tag):
    job_t(event, request, policy),
    m_channel(channel),
    m_route(route),
    m_tag(tag)
{
    io::command<acknowledgement> pack(m_tag);
    m_channel.send(m_route.front(), pack);
}

void dealer_job_t::react(const events::chunk& event) {
    io::command<chunk> pack(m_tag, event.message);
    m_channel.send(m_route.front(), pack);
}

void dealer_job_t::react(const events::error& event) {
    io::command<error> pack(m_tag, event.code, event.message);
    m_channel.send(m_route.front(), pack);
}

void dealer_job_t::react(const events::choke& event) {
    io::command<choke> pack(m_tag);
    m_channel.send(m_route.front(), pack);
}
