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

#include <swarm/urlfetcher/stream.hpp>

namespace cocaine { namespace service { namespace rest {

//!@todo: Handle with swarm requests (move them, probably).
template<typename T>
struct request_handler_t {
    typedef std::function<void(cocaine::deferred<T>&, int code, const std::string&)> Callback;

    cocaine::deferred<T> deferred;
    Callback callback;

    request_handler_t(cocaine::deferred<T> deferred, Callback callback) :
        deferred(deferred),
        callback(callback)
    {}

    void
    operator()(const ioremap::swarm::url_fetcher::response& reply,
               const std::string& data,
               const boost::system::error_code& error) {
        const int code = reply.code();
        const bool success = (!error && (code < 400 || code >= 600));

        if (!success) {
            if (code == 0) {
                // Socket-only error, no valid http response
                const std::string &message = cocaine::format("Unable to download {}, error {}",
                                                             reply.request().url().to_string(),
                                                             error.message());
                //deferred.abort(error.value(), message);
                deferred.abort(asio::error::operation_aborted, message);
                return;
            }
        }
        callback(deferred, code, data);
    }
};

template<typename T>
class Get {
    ioremap::swarm::url_fetcher &manager;

public:
    Get(ioremap::swarm::url_fetcher &manager) :
        manager(manager)
    {}

    void
    operator()(ioremap::swarm::url_fetcher::request request, request_handler_t<T> handler) {
        manager.get(ioremap::swarm::simple_stream::create(handler), std::move(request));
    }
};

template<typename T>
class Post {
    ioremap::swarm::url_fetcher &manager;
    std::string body;

public:
    Post(ioremap::swarm::url_fetcher &manager, const std::string& body) :
        manager(manager),
        body(body)
    {}

    void
    operator()(ioremap::swarm::url_fetcher::request request, request_handler_t<T> handler) {
        manager.post(ioremap::swarm::simple_stream::create(handler), std::move(request), std::move(body));
    }
};

#ifdef ELASTICSEARCH_DELETE_SUPPORT
template<typename T>
class Delete {
    ioremap::swarm::url_fetcher &manager;

public:
    Delete(ioremap::swarm::url_fetcher& manager) :
        manager(manager)
    {}

    void
    operator()(ioremap::swarm::url_fetcher::request request, request_handler_t<T> handler) {
        manager.do_delete(handler, request);
    }
};
#endif

} } }
