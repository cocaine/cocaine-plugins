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

#include <functional>
#include <stdexcept>
#include <tuple>

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/traits/tuple.hpp>

#include <blackhole/logger.hpp>

#include <swarm/urlfetcher/boost_event_loop.hpp>
#include <swarm/urlfetcher/url_fetcher.hpp>

#include "cocaine/service/elasticsearch/config.hpp"
#include "cocaine/service/elasticsearch.hpp"

#include "rest/delete.hpp"
#include "rest/get.hpp"
#include "rest/handlers.hpp"
#include "rest/index.hpp"
#include "rest/search.hpp"

using namespace ioremap;

using namespace cocaine::service;
using namespace cocaine::service::rest;

namespace ph = std::placeholders;

class elasticsearch_t::impl_t {
public:
    std::string m_url_prefix;
    boost::asio::io_service loop;
    swarm::boost_event_loop m_loop;
    swarm::logger m_logger;
    mutable swarm::url_fetcher m_manager;
    const std::string m_endpoint;
    std::shared_ptr<logging::logger_t> m_log;

    impl_t(cocaine::context_t &context,
           asio::io_service &asio,
           const std::string &name,
           const cocaine::dynamic_t& args) :
        m_loop(loop),
        m_manager(m_loop, m_logger),
        m_endpoint(extract_endpoint(args)),
        m_log(context.log(name))
    {
        COCAINE_LOG_INFO(m_log, "Elasticsearch endpoint: {}", m_endpoint);
    }

    template<typename T, typename Handler>
    cocaine::deferred<T>
    do_get(const std::string& url, Handler handler) const {
        Get<T> action(m_manager);
        return do_rest<T>(url, handler, action);
    }

    template<typename T, typename Handler>
    cocaine::deferred<T>
    do_post(const std::string& url, const std::string& body, Handler handler) const {
        Post<T> action { m_manager, body };
        return do_rest<T>(url, handler, action);
    }

    template<typename T, typename Handler>
    cocaine::deferred<T>
    do_delete(const std::string& url, Handler handler) const {
#ifdef ELASTICSEARCH_DELETE_SUPPORT
        Delete<T> action { m_manager };
        return do_rest<T>(url, handler, action);
#else
        cocaine::deferred<response::delete_index> deferred;
        deferred.abort(asio::error::operation_aborted, "Delete operation is not supported");
        return deferred;
#endif
    }

    template<typename T, typename Handler, typename Action>
    cocaine::deferred<T>
    do_rest(const std::string& url, Handler handler, Action action) const {
        cocaine::deferred<T> deferred;
        request_handler_t<T> request_handler(deferred, handler);

        swarm::url_fetcher::request request;
        request.set_url(url);
        action(request, request_handler);
        return deferred;
    }

private:
    static
    std::string
    extract_endpoint(const cocaine::dynamic_t& args) {
        const std::string& host = args.as_object().at("host", "127.0.0.1").as_string();
        const uint16_t port = args.as_object().at("port", 9200).to<uint16_t>();
        return cocaine::format("http://{}:{}", host, port);
    }
};

elasticsearch_t::elasticsearch_t(cocaine::context_t& context,
                                 asio::io_service& asio,
                                 const std::string& name,
                                 const dynamic_t& args) :
    service_t(context, asio, name, args),
    dispatch<io::elasticsearch_tag>(name),
    d(new impl_t(context, asio, name, args))
{
    on<io::elasticsearch::get>(std::bind(&elasticsearch_t::get, this, ph::_1, ph::_2, ph::_3));
    on<io::elasticsearch::index>(std::bind(&elasticsearch_t::index, this, ph::_1, ph::_2, ph::_3, ph::_4));
    on<io::elasticsearch::search>(std::bind(&elasticsearch_t::search, this, ph::_1, ph::_2, ph::_3, ph::_4));
    on<io::elasticsearch::delete_index>(std::bind(&elasticsearch_t::delete_index, this, ph::_1, ph::_2, ph::_3));
}

elasticsearch_t::~elasticsearch_t() {
}

cocaine::deferred<response::get>
elasticsearch_t::get(const std::string& index,
                     const std::string& type,
                     const std::string& id) const {
    const std::string& url = cocaine::format("{}/{}/{}/{}/", d->m_endpoint, index, type, id);
    get_handler_t handler { d->m_log };
    return d->do_get<response::get>(url, handler);
}

cocaine::deferred<response::index>
elasticsearch_t::index(const std::string& data,
                       const std::string& index,
                       const std::string& type,
                       const std::string& id) const {
    const std::string& url = cocaine::format("{}/{}/{}/{}", d->m_endpoint, index, type, id);
    index_handler_t handler { d->m_log };
    return d->do_post<response::index>(url, data, handler);
}

cocaine::deferred<response::search>
elasticsearch_t::search(const std::string& index,
                        const std::string& type,
                        const std::string& query,
                        int size) const {
    if (size <= 0) {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument));
        //throw cocaine::error_t("desired search size (%d) must be positive number", size);
    }

    const std::string& url = cocaine::format("{}/{}/{}/_search?q={}&size={}", d->m_endpoint, index, type, query, size);
    search_handler_t handler { d->m_log };
    return d->do_get<response::search>(url, handler);
}

cocaine::deferred<response::delete_index>
elasticsearch_t::delete_index(const std::string& index,
                              const std::string& type,
                              const std::string& id) const {
    const std::string& url = cocaine::format("{}/{}/{}/{}", d->m_endpoint, index, type, id);
    delete_handler_t handler { d->m_log };
    return d->do_delete<response::delete_index>(url, handler);
}
