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

#include <boost/format.hpp>

#include "job.hpp"

using namespace cocaine::engine;
using namespace cocaine::driver;

namespace {
    struct cocaine_response_t {
        int code;

        typedef std::vector<
            std::pair<
                std::string,
                std::string
            >
        > header_vector_t;

        header_vector_t headers;
    };
}

namespace cocaine { namespace io {
    template<>
    struct type_traits<cocaine_response_t> {
        static
        void
        unpack(const msgpack::object& packed,
               cocaine_response_t& object)
        {
            if (packed.type != msgpack::type::MAP) { 
                throw msgpack::type_error();
            }

            msgpack::object_kv * ptr = packed.via.map.ptr,
                               * const end = packed.via.map.ptr + packed.via.map.size;

            for (; ptr < end; ++ptr) {
                std::string key;

                // Get the key.
                ptr->key.convert(&key);

                if(!key.compare("code")) {
                    ptr->val.convert(&object.code);
                } else if(!key.compare("headers")) {
                    ptr->val.convert(&object.headers);
                }
            }
        }
    };
}}

blastbeat_job_t::blastbeat_job_t(const std::string& event, 
                                 const std::string& request,
                                 const policy_t& policy,
                                 const std::string& sid,
                                 io::socket_t& socket):
    job_t(event, request, policy),
    m_sid(sid),
    m_socket(socket),
    m_body(false)
{ }

void
blastbeat_job_t::react(const events::chunk& event) {
    if(!m_body) {
        msgpack::unpacked unpacked;
        cocaine_response_t response;

        try {
            msgpack::unpack(
                &unpacked,
                static_cast<const char*>(event.message.data()),
                event.message.size()
            );
            
            io::type_traits<cocaine_response_t>::unpack(
                unpacked.get(),
                response
            );
        } catch(const msgpack::type_error& e) {
            return;
        } catch(const std::bad_cast& e) {
            return;
        }

        boost::format code("HTTP/1.1 %d\r\n"),
                      header("%s: %s\r\n");

        std::string type("headers"),
                    body = (code % response.code).str();
       
        for(cocaine_response_t::header_vector_t::const_iterator it = response.headers.begin();
            it != response.headers.end();
            ++it)
        {
            body += (header % it->first % it->second).str();
            header.clear();
        }

        body += (header % "Connection" % "close").str();
        body += "\r\n";

        m_socket.send(io::protect(m_sid), ZMQ_SNDMORE);
        m_socket.send(io::protect(type), ZMQ_SNDMORE);
        m_socket.send(io::protect(body));

        m_body = true;
    } else {
        std::string type("body");
        
        m_socket.send(io::protect(m_sid), ZMQ_SNDMORE);
        m_socket.send(io::protect(type), ZMQ_SNDMORE);
        m_socket.send(event.message);
    }
}

void
blastbeat_job_t::react(const events::error& event) {
    std::string type("headers");

}

void
blastbeat_job_t::react(const events::choke& event) {
    std::string type("end"),
                empty;
    
    m_socket.send(io::protect(m_sid), ZMQ_SNDMORE);
    m_socket.send(io::protect(type), ZMQ_SNDMORE);
    m_socket.send(io::protect(empty));
}
