/*
    Copyright (c) 2011-2013 Evgeny Safronov <esafronov@yandex-team.ru>
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

#include "zmqsocket.hpp"

#include <cocaine/common.hpp>
#include <cocaine/asio/socket.hpp>

using namespace cocaine::driver;

zmq_socket::zmq_socket() :
    m_zmq_context(1),
    m_socket(m_zmq_context, ZMQ_ROUTER)
{
}

int
zmq_socket::get_fd()
{
    int fd = 0;
    size_t fd_size = sizeof(fd);
    m_socket.getsockopt(ZMQ_FD, &fd, &fd_size);
    return fd;
}

unsigned long zmq_socket::get_events()
{
    unsigned long events = 0;
    size_t size = sizeof(events);
    m_socket.getsockopt(ZMQ_EVENTS, &events, &size);
    return events;
}

void
zmq_socket::set_receive_timeout(int timeout)
{
    m_socket.setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
}

int
zmq_socket::get_receive_timeout()
{
    int receive_timeout = 0;
    size_t size = sizeof(receive_timeout);
    m_socket.getsockopt(ZMQ_RCVTIMEO, &receive_timeout, &size);
    return receive_timeout;
}

std::string
zmq_socket::get_last_endpoint()
{
    char last_endpoint[64];
    size_t size = sizeof(last_endpoint);
    m_socket.getsockopt(ZMQ_LAST_ENDPOINT, &last_endpoint, &size);
    return std::string(last_endpoint, size);
}

bool zmq_socket::has_more()
{
    int64_t rcvmore = 0;
    size_t size = sizeof(rcvmore);
    m_socket.getsockopt(ZMQ_RCVMORE, &rcvmore, &size);
    return rcvmore != 0;
}

void zmq_socket::drop()
{
    zmq::message_t null;
    while(has_more()) {
        m_socket.recv(&null);
    }
}
