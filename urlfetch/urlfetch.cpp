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

#include "urlfetch.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include <tuple>

using namespace cocaine;
using namespace cocaine::service;
using namespace std::placeholders;
using namespace ioremap;

urlfetch_t::urlfetch_t(context_t& context,
                       cocaine::io::reactor_t &reactor,
                       const std::string& name,
                       const Json::Value& args):
    service_t(context, reactor, name, args),
    m_context(context),
    m_manager(reactor.native())
{
    on<io::urlfetch::get>("get", std::bind(&urlfetch_t::get, this, _1));
}

struct urlfetch_get_handler
{
    deferred<service::network_reply_t> handler;

    void operator() (const swarm::network_reply &reply)
    {
        handler.write(reply);
    }
};

deferred<service::network_reply_t> service::urlfetch_t::get(const service::network_request_t &request)
{
    urlfetch_get_handler handler;
    m_manager.get(handler, request);
    return handler.handler;
}
