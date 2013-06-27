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

#ifndef COCAINE_BLASTBEAT_DRIVER_HPP
#define COCAINE_BLASTBEAT_DRIVER_HPP

#include <boost/unordered_map.hpp>

#include <cocaine/api/driver.hpp>
#include <cocaine/asio/reactor.hpp>

#include <zmq.hpp>

namespace cocaine { namespace driver {

class blastbeat_t:
    public api::driver_t
{
    public:
        typedef api::driver_t category_type;

    public:
        blastbeat_t(context_t& context, io::reactor_t& reactor, app_t& app, const std::string& name, const Json::Value& args);

        virtual
       ~blastbeat_t();

        virtual
        Json::Value
        info() const;

        bool
        send(const std::string& sid, const std::string& type, const std::string& body) {
            zmq::message_t message;

            message.rebuild(sid.size());
            std::memcpy(message.data(), sid.data(), sid.size());
            m_socket.send(message, ZMQ_SNDMORE);

            message.rebuild(type.size());
            std::memcpy(message.data(), type.data(), type.size());
            m_socket.send(message, ZMQ_SNDMORE);

            message.rebuild(body.size());
            std::memcpy(message.data(), body.data(), body.size());
            m_socket.send(message);

            on_check(m_checker, ev::PREPARE);

            return true;
        }

    private:
        void
        on_event(ev::io&, int);

        void
        on_check(ev::prepare&, int);

        void
        process_events();

        void
        on_ping();

        void
        on_spawn();

        void
        on_uwsgi(const std::string& sid, zmq::message_t& message);

        void
        on_body(const std::string& sid, zmq::message_t& message);

        void
        on_end(const std::string& sid);

    protected:
        context_t& m_context;
        std::unique_ptr<logging::log_t> m_log;

        // Event loop

        io::reactor_t& m_reactor;

        ev::io m_watcher;
        ev::prepare m_checker;

        // Scheduler

        app_t& m_app;

        // Configuration

        const std::string m_event;
        const std::string m_endpoint;

        // I/O

        zmq::context_t m_zmq;
        zmq::socket_t m_socket;

        // Session tracking

        typedef boost::unordered_map<
            std::string,
            std::shared_ptr<api::stream_t>
        > stream_map_t;

        stream_map_t m_streams;
};

}}

#endif
