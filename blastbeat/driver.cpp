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

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/format.hpp>
#include <boost/tokenizer.hpp>

#include <cocaine/context.hpp>
#include <cocaine/engine.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/session.hpp>

#include "driver.hpp"
#include "job.hpp"

using namespace cocaine;
using namespace cocaine::driver;

blastbeat_t::blastbeat_t(context_t& context,
                         const std::string& name,
                         const Json::Value& args,
                         engine::engine_t& engine):
    category_type(context, name, args, engine),
    m_context(context),
    m_log(context.log(
        (boost::format("app/%1%")
            % name
        ).str()
    )),
    m_event(args.get("emit", "").asString()),
    m_identity(args.get("identity", "").asString()),
    m_endpoint(args.get("endpoint", "").asString()),
    m_loop(engine.loop()),
    m_watcher(engine.loop()),
    m_checker(engine.loop()),
    m_socket(context, ZMQ_DEALER)
{
    try {
        m_socket.setsockopt(
            ZMQ_IDENTITY,
            m_identity.data(),
            m_identity.size()
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

    result["identity"] = m_identity;
    result["endpoint"] = m_endpoint;
    result["type"] = "blastbeat";
    result["sessions"] = static_cast<Json::LargestUInt>(m_jobs.size());

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
    m_loop.feed_fd_event(m_socket.fd(), ev::READ);
}

namespace {
    typedef boost::tuple<
        io::raw<std::string>,
        io::raw<std::string>,
        zmq::message_t*
    > request_proxy_t;

    struct uwsgi_header_t {
        uint8_t  modifier1;
        uint16_t datasize;
        uint8_t  modifier2;
    } __attribute__((__packed__));
}

void
blastbeat_t::process_events() {
    int counter = defaults::io_bulk_size;
    
    std::string sid,
                type;

    zmq::message_t message;
   
    do {
        request_proxy_t proxy(
            io::protect(sid),
            io::protect(type),
            &message
        );

        {
            io::scoped_option<
                io::options::receive_timeout,
                io::policies::unique
            > option(m_socket, 0);
            
            // Try to read the next RPC command from the bus in a
            // non-blocking fashion. If it fails, break the loop.
            if(!m_socket.recv_tuple(proxy)) {
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
    } while(--counter);
}

void
blastbeat_t::on_ping() {
    std::string empty;
    send("", "pong", io::protect(empty));
}

void
blastbeat_t::on_spawn() {
    COCAINE_LOG_INFO(m_log, "connected to the blastbeat server");
}

void
blastbeat_t::on_uwsgi(const std::string& sid,
                      zmq::message_t& message)
{
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

    packer.pack_map(2);

    packer << std::string("meta") << env;

    // Parse the query string.
    // TODO: Drop this shit from here -->
    typedef boost::tokenizer<
        boost::char_separator<char>
    > tokenizer_type;

    boost::char_separator<char> separator("&");
    tokenizer_type tokenizer(env["QUERY_STRING"], separator);

    std::map<
        std::string,
        std::string
    > query_map;

    for(tokenizer_type::const_iterator it = tokenizer.begin();
        it != tokenizer.end();
        ++it)
    {
        std::vector<std::string> pair;
        boost::algorithm::split(pair, *it, boost::is_any_of("="));
        query_map[pair[0]] = pair[1];
    }

    packer << std::string("request") << query_map;
    // <-- to there.

    boost::shared_ptr<blastbeat_job_t> job(
        boost::make_shared<blastbeat_job_t>(
            m_event,
            sid,
            *this
        )
    );

    job_map_t::iterator it;

    boost::tie(it, boost::tuples::ignore) = m_jobs.emplace(
        sid,
        engine().enqueue(job)
    );

    it->second->push(
        std::string(
            buffer.data(),
            buffer.size()
        )
    );
}

void
blastbeat_t::on_body(const std::string& sid, 
                     zmq::message_t& body)
{
    job_map_t::iterator it(
        m_jobs.find(sid)
    );

    if(it == m_jobs.end()) {
        COCAINE_LOG_WARNING(m_log, "received an unknown session body");
        return;
    }

    it->second->push(
        std::string(
            static_cast<const char*>(body.data()),
            body.size()
        )
    );
}

void
blastbeat_t::on_end(const std::string& sid) {
    job_map_t::iterator it(
        m_jobs.find(sid)
    );

    if(it == m_jobs.end()) {
        COCAINE_LOG_WARNING(m_log, "received an unknown session termination");
        return;
    }

    m_jobs.erase(it);
}
