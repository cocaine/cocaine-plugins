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

#include "stream.hpp"

#include "driver.hpp"

#include <cocaine/traits.hpp>

#include <boost/format.hpp>

using namespace cocaine;
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
        unpack(const msgpack::object& packed, cocaine_response_t& object) {
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

blastbeat_stream_t::blastbeat_stream_t(blastbeat_t& driver, const std::string& sid):
    m_driver(driver),
    m_sid(sid),
    m_body(false)
{ }

void
blastbeat_stream_t::write(const char * chunk, size_t size) {
    if(!m_body) {
        msgpack::unpacked unpacked;
        cocaine_response_t response;

        try {
            msgpack::unpack(&unpacked, chunk, size);

            io::type_traits<cocaine_response_t>::unpack(
                unpacked.get(),
                response
            );
        } catch(const msgpack::type_error& e) {
            return;
        } catch(const std::bad_cast& e) {
            return;
        }

        // TODO: Use proper HTTP version.
        std::string body = cocaine::format("HTTP/1.0 %d\r\n", response.code);

        boost::format header("%s: %s\r\n");

        for(cocaine_response_t::header_vector_t::const_iterator it = response.headers.begin();
            it != response.headers.end();
            ++it)
        {
            body += (header % it->first % it->second).str();
            header.clear();
        }

        // TODO: Support Keep-Alive connections.
        body += (header % "Connection" % "close").str();
        body += "\r\n";

        m_driver.send(m_sid, std::string("headers"), body);

        m_body = true;
    } else {
        m_driver.send(m_sid, std::string("body"), std::string(chunk, size));
    }
}

void
blastbeat_stream_t::error(int, const std::string&) {
    std::string empty;

    // TODO: Proper error reporting.
    m_driver.send(m_sid, std::string("retry"), empty);
}

void
blastbeat_stream_t::close() {
    std::string empty;

    m_driver.send(m_sid, std::string("end"), empty);
}
