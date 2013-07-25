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
