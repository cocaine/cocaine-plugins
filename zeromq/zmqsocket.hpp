#pragma once

#include <string>

#include <zmq.hpp>

#include <ev++.h>
#include <zmq.hpp>

#include <cocaine/common.hpp>

#include <cocaine/asio/socket.hpp>

#include <cocaine/api/driver.hpp>

namespace cocaine { namespace driver {

class zmq_socket {
public:
    explicit zmq_socket();

    void bind(const char *endpoint) {
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
    get_events() {
        unsigned long events = 0;
        size_t size = sizeof(events);
        m_socket.getsockopt(ZMQ_EVENTS, &events, &size);
        return events;
    }

    void set_receive_timeout(int timeout) {
        m_socket.setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    }

    int
    get_receive_timeout() {
        int receive_timeout = 0;
        size_t size = sizeof(receive_timeout);
        m_socket.getsockopt(ZMQ_RCVTIMEO, &receive_timeout, &size);
        return receive_timeout;
    }

    bool
    has_more();

    void
    drop();

private:
    zmq::context_t m_zmq_context;
    zmq::socket_t m_socket;
};

} }
