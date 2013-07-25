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

#ifndef COCAINE_ZEROMQ_SERVER_DRIVER_HPP
#define COCAINE_ZEROMQ_SERVER_DRIVER_HPP

#include <ev++.h>
#include <zmq.hpp>

#include <cocaine/common.hpp>

#include "zmqsocket.hpp"

namespace cocaine { namespace driver {

class zmq_t:
    public api::driver_t
{
    public:
        typedef api::driver_t category_type;

    public:
        zmq_t(context_t& context, io::reactor_t& reactor, app_t& app, const std::string& name, const Json::Value& args);

        virtual
       ~zmq_t();

        virtual
        Json::Value
        info() const;

        template<class T>
        bool
        send(const std::string& route, T&& message) {
            const std::string empty;

            on_check(m_checker, ev::PREPARE);
            return m_zmq_socket.send_multipart(route, empty, message);
        }

    private:
        void
        on_event(ev::io&, int);

        void
        on_check(ev::prepare&, int);

        void
        process_events();


    private:
        context_t& m_context;
        io::reactor_t& m_reactor;
        app_t& m_app;
        std::shared_ptr<logging::log_t> m_log;

        // Configuration

        const std::string m_event;

        // I/O
        std::string m_endpoint;
        zmq_socket m_zmq_socket;

        // Event loop

        ev::io m_watcher;
        ev::prepare m_checker;
};

}}

#endif
