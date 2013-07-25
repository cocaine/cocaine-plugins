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

#pragma once

#include <string>

#include <ev++.h>
#include <zmq.hpp>

#include <cocaine/api/driver.hpp>

namespace cocaine { namespace driver {

class zmq_socket {
public:
    explicit zmq_socket();

    inline void
    bind(const char *endpoint) {
        m_socket.bind(endpoint);
    }

    template<typename Head>
    bool
    send_multipart(Head&& head) {
        return m_socket.send(head);
    }

    template<typename Head, typename... Tail>
    bool
    send_multipart(Head&& head, Tail&&... tail) {
        return send(head, ZMQ_SNDMORE) && send_multipart(std::forward<Tail>(tail)...);
    }

    inline bool
    send(const std::string& blob, int flags = 0) {
        zmq::message_t message(blob.size());
        memcpy(message.data(), blob.data(), blob.size());
        return m_socket.send(message, flags);
    }

    inline bool recv(zmq::message_t *message) {
        return m_socket.recv(message);
    }

    int
    get_fd();

    unsigned long
    get_events();

    void
    set_receive_timeout(int timeout);

    int
    get_receive_timeout();

    std::string
    get_last_endpoint();

    bool
    has_more();

    void
    drop();

private:
    zmq::context_t m_zmq_context;
    zmq::socket_t m_socket;
};

} }
