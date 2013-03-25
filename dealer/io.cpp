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

//#include "cocaine/io.hpp"
#include "io.hpp"
#include "cocaine/context.hpp"


#include<iostream>

#define COCAINE_EINTR_GUARD(command)        \
    do {                                    \
        try {                               \
            command;                        \
        } catch(const zmq::error_t& e) {    \
            if(e.num() != EINTR) {          \
                throw;                      \
            }                               \
        }                                   \
    } while(true);

using namespace cocaine::io;

socket_base_t::socket_base_t(context_t& context,
                             int type) :
    m_socket(zmq_single::instance()->get_context(), type),
    m_context(context)
{
    int linger = 0;
   
    // Disable the socket lingering on context termination. 
    m_socket.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

    size_t size = sizeof(m_fd);
    
    // Get the socket's file descriptor.
    m_socket.getsockopt(ZMQ_FD, &m_fd, &size);
} 

socket_base_t::~socket_base_t() {
    //Empty
}

void
socket_base_t::bind() {

    std::string endpoint = "tcp://*:*";

    m_socket.bind(
        "tcp://*:*"
    );

    size_t size = 255;
    char endpt[size];
    m_socket.getsockopt(ZMQ_LAST_ENDPOINT, &endpt, &size);
    m_endpoint = endpt;
    std::cerr << "Endpoint" << m_endpoint << std::endl;
}

void
socket_base_t::bind(const std::string& endpoint) {
    m_socket.bind(endpoint.c_str());

    // Try to determine the connection string for clients.
    // FIXME: Fix it when migrating to ZeroMQ 3.1+
    size_t position = endpoint.find_last_of(":");

    if(position != std::string::npos) {
        m_endpoint = std::string("tcp://") +
                     m_context.config.network.hostname +
                     endpoint.substr(position, std::string::npos);
    } else {
        m_endpoint = "<local>";
    }
}

bool
socket_base_t::send(zmq::message_t& message,
                    int flags)
{
    COCAINE_EINTR_GUARD(
        return m_socket.send(message, flags)
    );
}

bool
socket_base_t::recv(zmq::message_t& message,
                    int flags)
{
    COCAINE_EINTR_GUARD(
        return m_socket.recv(&message, flags)
    );
}

void
socket_base_t::getsockopt(int name,
                          void * value,
                          size_t * size)
{
    COCAINE_EINTR_GUARD(
        return m_socket.getsockopt(name, value, size)
    );
}

void
socket_base_t::setsockopt(int name,
                          const void * value,
                          size_t size)
{
    COCAINE_EINTR_GUARD(
        return m_socket.setsockopt(name, value, size)
    );
}

void
socket_base_t::drop() {
    zmq::message_t null;

    while(more()) {
        recv(null);
    }
}
