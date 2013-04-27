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

#include "service.hpp"

#include <cocaine/asio/socket.hpp>
#include <cocaine/asio/udp.hpp>

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

using namespace cocaine::io;
using namespace cocaine::service;

gateway_t::gateway_t(context_t& context,
                     io::reactor_t& reactor,
                     const std::string& name,
                     const Json::Value& args):
    category_type(context, reactor, name, args),
    m_log(new logging::log_t(context, name))
{
    if(::ipvs_init() != 0) {
        throw configuration_error_t(
            "unable to initialize the IP virtual server system - [%d] %s",
            errno,
            ::ipvs_strerror(errno)
        );
    }

    COCAINE_LOG_INFO(m_log, "using IP virtual server version %d", ::ipvs_version());

    m_sink.reset(new io::socket<udp>());

    const int loop = IP_DEFAULT_MULTICAST_LOOP;
    const int ttl  = args.get("ttl", IP_DEFAULT_MULTICAST_TTL).asInt();

    group_req request;

    std::memset(&request, 0, sizeof(request));

    request.gr_interface = 0;

    try {
        // Port number doesn't have any meaning here.
        udp::endpoint group(args["group"].asString(), 0);

        COCAINE_LOG_INFO(m_log, "joining multicast group '%s'", group.address());

        std::memcpy(&request.gr_group, group.data(), group.size());

        // NOTE: I don't think these calls might fail at all.
        ::setsockopt(m_sink->fd(), IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
        ::setsockopt(m_sink->fd(), IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

        if(::setsockopt(m_sink->fd(), IPPROTO_IP, MCAST_JOIN_GROUP, &request, sizeof(request)) != 0) {
            throw std::system_error(errno, std::system_category(), "unable to join a multicast group");
        }
    } catch(...) {
        ::ipvs_close();
        throw;
    }
}

gateway_t::~gateway_t() {
    ::ipvs_close();
}

