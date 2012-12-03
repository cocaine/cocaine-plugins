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

#include "service.hpp"

#include <boost/bind.hpp>
#include <boost/tuple/tuple.hpp>

using namespace cocaine;
using namespace cocaine::service;

logging_t::logging_t(context_t& context,
                     const std::string& name,
                     const Json::Value& args):
    category_type(context, name, args),
    api::reactor<io::tags::logging_tag>(context, name, args),
    m_context(context)
{
    on<io::service::emit>(
        boost::bind(&logging_t::on_emit, this, _1, _2, _3)
    );

    COCAINE_LOG_INFO(
        m_context.log("logging"),
        "the service has started"
    );
}

void
logging_t::run() {
    loop();
}

void
logging_t::terminate() {
    // TODO: No-op.
}

void
logging_t::on_emit(int priority,
                   std::string source,
                   std::string message)
{
    log_map_t::iterator it = m_logs.find(source);

    if(it == m_logs.end()) {
        boost::tie(it, boost::tuples::ignore) = m_logs.emplace(
            source,
            m_context.log(source)
        );
    }

    COCAINE_LOG(
        it->second,
        static_cast<logging::priorities>(priority),
        "%s",
        message
    );
}
