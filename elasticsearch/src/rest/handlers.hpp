/*
    Copyright (c) 2011-2013 Evgeny Safronov <esafronov@yandex-team.ru>
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

#pragma once

#include <cocaine/service/elasticsearch.hpp>

namespace cocaine { namespace service { namespace rest {

template<typename T>
struct request_handler_t {
    typedef std::function<void(cocaine::deferred<T>, int code, const std::string&)> Callback;

    cocaine::deferred<T> deferred;
    Callback callback;

    request_handler_t(cocaine::deferred<T> deferred, Callback callback) :
        deferred(deferred),
        callback(callback)
    {}

    void operator()(const ioremap::swarm::network_reply& reply) {
        const std::string data = reply.get_data();
        const int code = reply.get_code();
        const bool success = (reply.get_error() == 0 && (code < 400 || code >= 600));

        if (success) {
        } else {
            if (code == 0) {
                // Socket-only error, no valid http response
                const std::string &message = cocaine::format("Unable to download %s, network error code %d",
                                                             reply.get_request().get_url(),
                                                             reply.get_error());
                return deferred.abort(-reply.get_error(), message);
            }
        }
        callback(deferred, code, data);
    }
};

template<typename T>
struct Get {
    const ioremap::swarm::network_manager &manager;

    void operator()(const ioremap::swarm::network_request &request, request_handler_t<T> handler) {
        manager.get(handler, request);
    }
};

template<typename T>
struct Post {
    const ioremap::swarm::network_manager &manager;
    std::string body;

    void operator()(ioremap::swarm::network_request request, request_handler_t<T> handler) {
        manager.post(handler, request, body);
    }
};

template<typename T>
struct Delete {
    const ioremap::swarm::network_manager &manager;

    void operator()(ioremap::swarm::network_request request, request_handler_t<T> handler) {
        manager.do_delete(handler, request);
    }
};

} } }
