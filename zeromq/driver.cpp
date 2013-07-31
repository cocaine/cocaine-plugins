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

#include <cocaine/context.hpp>
#include <cocaine/app.hpp>
#include <cocaine/asio/reactor.hpp>

#include <cocaine/logging.hpp>

#include <cocaine/api/event.hpp>

#include "driver.hpp"
#include "stream.hpp"
#include "utils.hpp"

using namespace cocaine;
using namespace cocaine::driver;
using namespace cocaine::logging;

const std::string DEFAULT_ENDPOINT = "tcp://*:*";

zmq_t::zmq_t(context_t& context, io::reactor_t& reactor, app_t& app, const std::string& name, const Json::Value& args):
    category_type(context, reactor, app, name, args),
    m_context(context),
    m_reactor(reactor),
    m_app(app),
    m_log(new log_t(context, cocaine::format("app/%s", name))),
    m_event(args["emit"].asString()),
    m_endpoint(args["endpoint"].asString()),
    m_watcher(reactor.native()),
    m_checker(reactor.native())
{
    if(m_endpoint.empty()) {
        m_endpoint = DEFAULT_ENDPOINT;
    }

    try {
        m_zmq_socket.bind(m_endpoint.c_str());
        m_endpoint = m_zmq_socket.get_last_endpoint();
    } catch(const zmq::error_t& e) {
        throw cocaine::error_t("invalid driver endpoint - %s", e.what());
    }

    m_watcher.set<zmq_t, &zmq_t::on_event>(this);
    m_watcher.start(m_zmq_socket.get_fd(), ev::READ);
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
    result["endpoint"] = m_endpoint;

    return result;
}

void
zmq_t::on_event(ev::io&, int) {
    m_checker.stop();

    const unsigned long event = ZMQ_POLLIN;
    const unsigned long events = m_zmq_socket.get_events();
    const bool pending = event & events;

    if(pending) {
        m_checker.start();
        process_events();
    }
}

void
zmq_t::on_check(ev::prepare&, int) {
    m_reactor.native().feed_fd_event(m_zmq_socket.get_fd(), ev::READ);
}

void
zmq_t::process_events() {
    int counter = 100;

    // Message origin.
    route_t route;

    // Temporary message buffer.
    zmq::message_t message;

    while(counter--) {
        route.clear();

        do {
            {
                timeout_watcher<zmq_socket> watcher(m_zmq_socket);
                if(!m_zmq_socket.recv(&message)) {
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
        } while(m_zmq_socket.has_more());

        if(route.empty() || !m_zmq_socket.has_more()) {
            COCAINE_LOG_ERROR(m_log, "received a corrupted request");
            m_zmq_socket.drop();
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
            downstream = m_app.enqueue(api::event_t(m_event), upstream);
        } catch(const cocaine::error_t& e) {
            upstream->error(resource_error, e.what());
        }

        do {
            m_zmq_socket.recv(&message);

            downstream->write(
                static_cast<const char*>(message.data()),
                message.size()
            );
        } while(m_zmq_socket.has_more());
    }
}

