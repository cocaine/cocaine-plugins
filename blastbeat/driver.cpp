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

#include "driver.hpp"

#include "stream.hpp"

#include <cocaine/app.hpp>
#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/traits.hpp>

#include <cocaine/api/event.hpp>

#include <tuple>

using namespace cocaine;
using namespace cocaine::driver;
using namespace cocaine::logging;

blastbeat_t::blastbeat_t(context_t& context, io::reactor_t& reactor, app_t& app, const std::string& name, const Json::Value& args):
    category_type(context, reactor, app, name, args),
    m_context(context),
    m_log(new log_t(context, cocaine::format("app/%s", name))),
    m_reactor(reactor),
    m_watcher(reactor.native()),
    m_checker(reactor.native()),
    m_app(app),
    m_event(args.get("emit", "").asString()),
    m_endpoint(args.get("endpoint", "").asString()),
    m_zmq(1),
    m_socket(m_zmq, ZMQ_DEALER)
{
    try {
        m_socket.setsockopt(
            ZMQ_IDENTITY,
            m_context.config.network.hostname.data(),
            m_context.config.network.hostname.size()
        );
    } catch(const zmq::error_t& e) {
        throw cocaine::error_t("invalid driver identity - %s", e.what());
    }

    int timeout = 0;
    size_t timeout_size = sizeof(timeout);

    m_socket.setsockopt(ZMQ_RCVTIMEO, &timeout, timeout_size);

    try {
        m_socket.connect(m_endpoint.c_str());
    } catch(const zmq::error_t& e) {
        throw cocaine::error_t("invalid driver endpoint - %s", e.what());
    }

    int fd = 0;
    size_t fd_size = sizeof(fd);

    // Get the socket's file descriptor.
    m_socket.getsockopt(ZMQ_FD, &fd, &fd_size);

    m_watcher.set<blastbeat_t, &blastbeat_t::on_event>(this);
    m_watcher.start(fd, ev::READ);
    m_checker.set<blastbeat_t, &blastbeat_t::on_check>(this);
    m_checker.start();
}

blastbeat_t::~blastbeat_t() {
    m_watcher.stop();
    m_checker.stop();
}

Json::Value
blastbeat_t::info() const {
    Json::Value result;

    result["type"] = "blastbeat";
    result["endpoint"] = m_endpoint;
    result["identity"] = m_context.config.network.hostname;
    result["sessions"] = static_cast<Json::LargestUInt>(m_streams.size());

    return result;
}

void
blastbeat_t::on_event(ev::io&, int) {
    m_checker.stop();

    unsigned long events = 0;
    size_t size = sizeof(events);

    m_socket.getsockopt(ZMQ_EVENTS, &events, &size);

    if(events & ZMQ_POLLIN) {
        m_checker.start();
        process_events();
    }
}

void
blastbeat_t::on_check(ev::prepare&, int) {
    int fd = 0;
    size_t size = sizeof(fd);

    m_socket.getsockopt(ZMQ_FD, &fd, &size);

    m_reactor.native().feed_fd_event(fd, ev::READ);
}

namespace {

struct uwsgi_header_t {
    uint8_t  modifier1;
    uint16_t datasize;
    uint8_t  modifier2;
} __attribute__((__packed__));

}

void
blastbeat_t::process_events() {
    int counter = 100;

    // RPC payload.
    std::string sid;
    std::string type;

    // Temporary message buffer.
    zmq::message_t message;

    while(counter--) {
        if(!m_socket.recv(&message)) {
            return;
        }

        sid.assign(static_cast<const char*>(message.data()), message.size());

        if(!m_socket.recv(&message)) {
            return;
        }

        type.assign(static_cast<const char*>(message.data()), message.size());

        if(!m_socket.recv(&message)) {
            return;
        }

        COCAINE_LOG_DEBUG(
            m_log,
            "received a blastbeat request, type: '%s', body size: %d bytes",
            type,
            message.size()
        );

        if(type == "ping") {
            on_ping();
        } else if(type == "spawn") {
            on_spawn();
        } else if(type == "uwsgi") {
            on_uwsgi(sid, message);
        } else if(type == "body") {
            on_body(sid, message);
        } else if(type == "end") {
            on_end(sid);
        } else {
            COCAINE_LOG_WARNING(
                m_log,
                "received an unknown message type '%s'",
                type
            );
        }
    }
}

void
blastbeat_t::on_ping() {
    std::string empty;

    send("", "pong", empty);
}

void
blastbeat_t::on_spawn() {
    COCAINE_LOG_INFO(m_log, "connected to a blastbeat server");
}

void
blastbeat_t::on_uwsgi(const std::string& sid, zmq::message_t& message) {
    std::shared_ptr<blastbeat_stream_t> upstream(
        std::make_shared<blastbeat_stream_t>(
            *this,
            sid
        )
    );

    stream_map_t::iterator it;

    try {
        std::tie(it, std::ignore) = m_streams.emplace(
            sid,
            m_app.enqueue(api::event_t(m_event), upstream)
        );
    } catch(const cocaine::error_t& e) {
        COCAINE_LOG_ERROR(m_log, "unable to enqueue an event - %s", e.what());
        return;
    }

    std::map<
        std::string,
        std::string
    > env;

    const char * ptr = static_cast<const char*>(message.data()),
               * const end = ptr + message.size();

    // NOTE: Skip the uwsgi header, as we already know the message
    // length and we don't need uwsgi modifiers.
    ptr += sizeof(uwsgi_header_t);

    // Parse the HTTP headers.
    while(ptr < end) {
        const uint16_t * ksz,
                       * vsz;

        ksz = reinterpret_cast<const uint16_t*>(ptr);
        ptr += sizeof(*ksz);

        std::string key(ptr, *ksz);
        ptr += *ksz;

        vsz = reinterpret_cast<const uint16_t*>(ptr);
        ptr += sizeof(*vsz);

        std::string value(ptr, *vsz);
        ptr += *vsz;

        env[key] = value;
    }

    // Serialize the headers.
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> packer(buffer);

    packer << env;

    try {
        it->second->write(
            buffer.data(),
            buffer.size()
        );
    } catch(const cocaine::error_t& e) {
        COCAINE_LOG_ERROR(m_log, "unable to push headers to a session - %s", e.what());
    }
}

void
blastbeat_t::on_body(const std::string& sid, zmq::message_t& body) {
    stream_map_t::iterator it(
        m_streams.find(sid)
    );

    if(it == m_streams.end()) {
        COCAINE_LOG_WARNING(m_log, "received an unknown session body");
        return;
    }

    try {
        it->second->write(
            static_cast<const char*>(body.data()),
            body.size()
        );
    } catch(const cocaine::error_t& e) {
        COCAINE_LOG_ERROR(m_log, "unable to push a body chunk to a session - %s", e.what());
    }
}

void
blastbeat_t::on_end(const std::string& sid) {
    stream_map_t::iterator it(
        m_streams.find(sid)
    );

    if(it == m_streams.end()) {
        COCAINE_LOG_WARNING(m_log, "received an unknown session termination");
        return;
    }

    try {
        it->second->close();
    } catch(const cocaine::error_t& e) {
        COCAINE_LOG_ERROR(m_log, "unable to close a session - %s", e.what());
    }

    m_streams.erase(it);
}
