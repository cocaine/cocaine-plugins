#include "zmqsocket.hpp"

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
