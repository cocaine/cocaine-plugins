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

#include "logger.hpp"

using namespace cocaine;
using namespace cocaine::logger;

remote_t::remote_t(context_t& context,
                   const std::string& name,
                   const Json::Value& args):
    category_type(context, name, args),
    m_watermark(args["fallback"].get("watermark", 0).asUInt64()),
    m_client(context, "logging", m_watermark),
    m_ring(m_watermark)
{
    config_t::component_t cfg = {
        args["fallback"].get("type", "unspecified").asString(),
        args["fallback"]["args"]
    };

    try {
        m_fallback = context.get<api::logger_t>(cfg.type, context, "cocaine", cfg.args);
    } catch(const api::repository_error_t& e) {
        // Ignore it, continue without a fallback logger.
    }
}

namespace {
    struct dump_t {
        dump_t(api::logger_ptr_t logger):
            m_logger(logger)
        { }

        template<class T>
        void
        operator()(const T& entry) {
            m_logger->emit(
                boost::get<0>(entry),
                boost::get<1>(entry),
                boost::get<2>(entry)
            );
        }

    private:
        api::logger_ptr_t m_logger;
    };
}

void
remote_t::emit(logging::priorities priority,
               const std::string& source,
               const std::string& message)
{
    m_ring.push_front(
        boost::make_tuple(priority, source, message)
    );

    bool success = m_client.send<io::service::emit>(
        static_cast<int>(priority),
        std::move(source),
        std::move(message)
    );

    if(!success && m_fallback) {
        std::for_each(m_ring.begin(), m_ring.end(), dump_t(m_fallback));
    }
}
