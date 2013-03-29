/*
* 2013+ Copyright (c) Ruslan Nigatullin <euroelessar@yandex.ru>
* All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*/

#include "urlfetch.hpp"

using namespace cocaine;
using namespace cocaine::io;
using namespace cocaine::service;

using namespace ioremap;

using namespace std::placeholders;

urlfetch_t::urlfetch_t(context_t& context,
                       reactor_t& reactor,
                       const std::string& name,
                       const Json::Value& args):
    service_t(context, reactor, name, args),
    m_manager(reactor.native())
{
    on<io::urlfetch::get>("get", std::bind(&urlfetch_t::get, this, _1, _2, _3));
}

namespace {
    struct urlfetch_get_handler {
        deferred<network_reply_t> promise;

        void
        operator()(const network_reply_t& reply) {
            promise.write(reply);
        }
    };
}

deferred<network_reply_t>
urlfetch_t::get(const std::string& url,
                const std::map<std::string, std::string>& headers,
                bool recursive)
{
    network_request_t request;

    request.url = url;
    request.follow_location = recursive;

    std::copy(
        headers.begin(),
        headers.end(),
        std::back_inserter(request.headers)
    );

    urlfetch_get_handler handler;

    m_manager.get(handler, request);

    return handler.promise;
}
