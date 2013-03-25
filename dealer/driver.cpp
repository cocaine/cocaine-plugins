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

#include <cocaine/traits/policy.hpp>

using namespace cocaine;
using namespace cocaine::driver;
using namespace cocaine::logging;

dealer_t::dealer_t(context_t& context,
                   const std::string& name,
                   const Json::Value& args,
                   engine::engine_t& engine):
    category_type(context, name, args, engine),
    m_context(context),
    m_log(new log_t(context, cocaine::format("app/%s", name))),
    m_event(args["emit"].asString()),
    m_identity(
        cocaine::format("%s/%s", m_context.config.network.hostname, name)
    ),
    m_channel(context, ZMQ_ROUTER, m_identity),
    m_watcher(engine.service().loop()),
    m_checker(engine.service().loop())
{
    
    std::string endpoint(args["endpoint"].asString());

    try {
        if(endpoint.empty()) {
            m_channel.bind();
        } else {
            m_channel.bind(endpoint);
        }
    } catch(const zmq::error_t& e) {
        throw configuration_error_t("invalid driver endpoint - %s", e.what());
    }

    m_watcher.set<dealer_t, &dealer_t::on_event>(this);
    m_watcher.start(m_channel.fd(), ev::READ);
    m_checker.set<dealer_t, &dealer_t::on_check>(this);
    m_checker.start();
    
}

dealer_t::~dealer_t() {
    m_watcher.stop();
    m_checker.stop();
}

Json::Value
dealer_t::info() const {
    Json::Value result;

    result["endpoint"] = m_channel.endpoint();
    result["identity"] = m_identity;
    result["type"] = "native-server";

    return result;
}

void
dealer_t::on_event(ev::io&, int) {
    m_checker.stop();

    if(m_channel.pending()) {
        m_checker.start();
        process_events();
    }
}

void
dealer_t::on_check(ev::prepare&, int) {
    engine().service().loop().feed_fd_event(m_channel.fd(), ev::READ);
}

void
dealer_t::process_events() {
    int counter = 100; 

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
                > option(m_channel, 0);

                if(!m_channel.recv(message)) {
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
        } while(m_channel.more());

        if(route.empty() || !m_channel.more()) {
            COCAINE_LOG_ERROR(m_log, "received a corrupted request", m_event);
            m_channel.drop();
            return;
        }

        COCAINE_LOG_DEBUG(m_log, "received a request from '%s'", route[0]);

        do {
            std::string tag;
            api::policy_t policy;

            try {
                m_channel.recv_multipart(tag, policy, message);
            } catch(const std::exception& e) {
                COCAINE_LOG_ERROR(
                    m_log,
                    "received a corrupted request - %s",
                    m_event,
                    e.what()
                );

                m_channel.drop();

                return;
            }

            COCAINE_LOG_DEBUG(
                m_log,
                "enqueuing event type '%s' with uuid: %s",
                m_event,
                tag
            );

            std::shared_ptr<dealer_stream_t> stream(
                std::make_shared<dealer_stream_t>(
                    *this,
                    route,
                    tag
                )
            );

            try {
                engine().enqueue(api::event_t(m_event, policy), stream)->write(
                    static_cast<const char*>(message.data()),
                    message.size()
                );
            } catch(const cocaine::error_t& e) {
                stream->error(resource_error, e.what());
            }
        } while(m_channel.more());
    }
}

