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

#include "driver.hpp"

#include "stream.hpp"

#include <cocaine/context.hpp>
#include <cocaine/engine.hpp>
#include <cocaine/logging.hpp>

#include <cocaine/api/event.hpp>

using namespace cocaine;
using namespace cocaine::driver;
using namespace cocaine::logging;

zmq_t::zmq_t(context_t& context,
             const std::string& name,
             const Json::Value& args,
             engine::engine_t& engine):
    category_type(context, name, args, engine),
    m_context(context),
    m_log(new log_t(context, cocaine::format("app/%s", name))),
    m_event(args["emit"].asString()),
    m_socket(context, ZMQ_ROUTER),
    m_watcher(engine.service().loop()),
    m_checker(engine.service().loop())
{
    std::string endpoint(args["endpoint"].asString());

    try {
        if(endpoint.empty()) {
            m_socket.bind();
        } else {
            m_socket.bind(endpoint);
        }
    } catch(const zmq::error_t& e) {
        throw configuration_error_t("invalid driver endpoint - %s", e.what());
    }

    m_watcher.set<zmq_t, &zmq_t::on_event>(this);
    m_watcher.start(m_socket.fd(), ev::READ);
    m_checker.set<zmq_t, &zmq_t::on_check>(this);
    m_checker.start();
}

zmq_t::~zmq_t() {
    m_watcher.stop();
    m_checker.stop();
}

Json::Value
zmq_t::info() const {
    Json::Value result;

    result["type"] = "zeromq-server";
    result["endpoint"] = m_socket.endpoint();

    return result;
}

void
zmq_t::on_event(ev::io&, int) {
    m_checker.stop();

    if(m_socket.pending()) {
        m_checker.start();
        process_events();
    }
}

void
zmq_t::on_check(ev::prepare&, int) {
    engine().service().loop().feed_fd_event(m_socket.fd(), ev::READ);
}

void
zmq_t::process_events() {
    int counter = defaults::io_bulk_size;

    // Message origin.
    route_t route;

    // Temporary message buffer.
    zmq::message_t message;

    while(counter--) {
        route.clear();

        do {
            {
                io::scoped_option<
                    io::options::receive_timeout
                > option(m_socket, 0);

                if(!m_socket.recv(message)) {
                    return;
                }
            }

            if(!message.size()) {
                break;
            }

            route.emplace_back(
                static_cast<const char*>(message.data()),
                message.size()
            );
        } while(m_socket.more());

        if(route.empty() || !m_socket.more()) {
            COCAINE_LOG_ERROR(m_log, "received a corrupted request");
            m_socket.drop();
            return;
        }

        std::shared_ptr<zmq_stream_t> upstream(
            std::make_shared<zmq_stream_t>(
                *this,
                m_log,
                route
            )
        );

        std::shared_ptr<api::stream_t> downstream;

        try {
            downstream = engine().enqueue(api::event_t(m_event), upstream);
        } catch(const cocaine::error_t& e) {
            upstream->error(resource_error, e.what());
        }

        do {
            m_socket.recv(message);

            downstream->push(
                static_cast<const char*>(message.data()),
                message.size()
            );
        } while(m_socket.more());
    }
}

