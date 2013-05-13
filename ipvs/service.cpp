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

#include "service.hpp"

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

extern "C" {
    #include "libipvs-1.25/libipvs.h"
}

using namespace cocaine::io;
using namespace cocaine::service;

gateway_t::gateway_t(context_t& context,
                     io::reactor_t& reactor,
                     const std::string& name,
                     const Json::Value& args):
    category_type(context, reactor, name, args),
    m_log(new logging::log_t(context, name))
{
    if(::ipvs_init() != 0) {
        throw configuration_error_t(
            "unable to initialize the IP virtual server system - [%d] %s",
            errno,
            ::ipvs_strerror(errno)
        );
    }

    COCAINE_LOG_INFO(m_log, "using IP virtual server version %d", ::ipvs_version());
}

gateway_t::~gateway_t() {
    ::ipvs_close();
}

