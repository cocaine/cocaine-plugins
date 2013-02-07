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

#include <boost/tuple/tuple.hpp>

using namespace cocaine;
using namespace cocaine::driver;
using namespace cocaine::logging;

blastbeat_t::blastbeat_t(context_t& context,
                         const std::string& name,
                         const Json::Value& args,
                         engine::engine_t& engine):
    category_type(context, name, args, engine),
    m_context(context),
    m_log(new log_t(context, cocaine::format("app/%s", name))),
    m_event(args.get("emit", "").asString()),
    m_endpoint(args.get("endpoint", "").asString()),
    m_socket(context, ZMQ_DEALER),
    m_watcher(engine.loop()),
    m_checker(engine.loop())
{
    try {
        m_socket.setsockopt(
            ZMQ_IDENTITY,
            m_context.config.network.hostname.data(),
            m_context.config.network.hostname.size()
        );
    } catch(const zmq::error_t& e) {
        throw configuration_error_t("invalid driver identity - %s", e.what());
    }

    try {
        m_socket.connect(m_endpoint);
    } catch(const zmq::error_t& e) {
        throw configuration_error_t("invalid driver endpoint - %s", e.what());
    }

    m_watcher.set<blastbeat_t, &blastbeat_t::on_event>(this);
    m_watcher.start(m_socket.fd(), ev::READ);
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

    if(m_socket.pending()) {
        m_checker.start();
        process_events();
    }
}

void
blastbeat_t::on_check(ev::prepare&, int) {
    engine().loop().feed_fd_event(m_socket.fd(), ev::READ);
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
    int counter = defaults::io_bulk_size;

    // RPC payload.
    std::string sid;
    std::string type;

    // Temporary message buffer.
    zmq::message_t message;

    while(counter--) {
        {
            io::scoped_option<
                io::options::receive_timeout
            > option(m_socket, 0);

            // Try to read the next RPC command from the bus in a
            // non-blocking fashion. If it fails, break the loop.
            if(!m_socket.recv_multipart(sid, type, message)) {
                return;
            }
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
blastbeat_t::on_uwsgi(const std::string& sid,
                      zmq::message_t& message)
{
    boost::shared_ptr<blastbeat_stream_t> upstream(
        boost::make_shared<blastbeat_stream_t>(
            *this,
            sid
        )
    );

    stream_map_t::iterator it;

    try {
        boost::tie(it, boost::tuples::ignore) = m_streams.emplace(
            sid,
            engine().enqueue(api::event_t(m_event), upstream)
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
        it->second->push(
            buffer.data(),
            buffer.size()
        );
    } catch(const cocaine::error_t& e) {
        COCAINE_LOG_ERROR(m_log, "unable to push data to a session - %s", e.what());
    }
}

void
blastbeat_t::on_body(const std::string& sid,
                     zmq::message_t& body)
{
    stream_map_t::iterator it(
        m_streams.find(sid)
    );

    if(it == m_streams.end()) {
        COCAINE_LOG_WARNING(m_log, "received an unknown session body");
        return;
    }

    try {
        it->second->push(
            static_cast<const char*>(body.data()),
            body.size()
        );
    } catch(const cocaine::error_t& e) {
        COCAINE_LOG_ERROR(m_log, "unable to push a chunk to a session - %s", e.what());
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
