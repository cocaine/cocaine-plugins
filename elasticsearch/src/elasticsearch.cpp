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
#include <tuple>
#include <stdexcept>

#include <boost/foreach.hpp>

#include <cocaine/traits/tuple.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/memory.hpp>

#include <swarm/urlfetcher/ev_event_loop.hpp>

#include "cocaine/service/elasticsearch/config.hpp"
#include "cocaine/service/elasticsearch.hpp"
#include "rest/handlers.hpp"
#include "rest/index.hpp"
#include "rest/get.hpp"
#include "rest/search.hpp"
#include "rest/delete.hpp"

#define BOOST_BIND_NO_PLACEHOLDERS
#include <blackhole/formatter/string.hpp>
#undef BOOST_BIND_NO_PLACEHOLDERS


using namespace std::placeholders;

using namespace ioremap;

using namespace cocaine;
using namespace cocaine::service;
using namespace cocaine::service::rest;

namespace
{
  class log_adapter_impl_t : public blackhole::base_frontend_t
  {
    public:
      log_adapter_impl_t(const std::shared_ptr<logging::log_t> &log);

      virtual void handle(const blackhole::log::record_t &record);

    private:
      std::shared_ptr<logging::log_t> m_log;
      blackhole::formatter::string_t m_formatter;
  };

  static cocaine::logging::priorities convert_verbosity(ioremap::swarm::log_level level)
  {
    switch (level) {
      case SWARM_LOG_DEBUG:
        return cocaine::logging::debug;
      case SWARM_LOG_NOTICE:
      case SWARM_LOG_INFO:
        return cocaine::logging::info;
      case SWARM_LOG_WARNING:
        return cocaine::logging::warning;
      case SWARM_LOG_ERROR:
        return cocaine::logging::error;
      default:
        return cocaine::logging::ignore;
    };
  }

  static ioremap::swarm::log_level convert_verbosity(cocaine::logging::priorities prio) {
    switch (prio) {
      case cocaine::logging::debug:
        return SWARM_LOG_DEBUG;
      case cocaine::logging::info:
        return SWARM_LOG_INFO;
      case cocaine::logging::warning:
        return SWARM_LOG_WARNING;
      case cocaine::logging::error:
      default:
        return SWARM_LOG_ERROR;
    }
  }

  log_adapter_impl_t::log_adapter_impl_t(const std::shared_ptr<logging::log_t> &log): m_log(log), m_formatter("%(message)s %(...L)s")
  {
  }

  void log_adapter_impl_t::handle(const blackhole::log::record_t &record)
  {
    swarm::log_level level = record.extract<swarm::log_level>(blackhole::keyword::severity<swarm::log_level>().name());
    auto cocaine_level = convert_verbosity(level);
    COCAINE_LOG(m_log, cocaine_level, "elliptics: %s", m_formatter.format(record));
  }
  
  class log_adapter_t : public ioremap::swarm::logger_base
  {
    public:
      log_adapter_t(const std::shared_ptr<logging::log_t> &log);
  };
  
  log_adapter_t::log_adapter_t(const std::shared_ptr<logging::log_t> &log)
  {
    add_frontend(std::make_unique<log_adapter_impl_t>(log));
    verbosity(convert_verbosity(log->verbosity()));
  }
}

class elasticsearch_t::impl_t {
public:
    std::string m_url_prefix;
    swarm::ev_event_loop m_loop;
    std::shared_ptr<logging::log_t> m_log;
    log_adapter_t m_logger_base;
    swarm::logger m_logger;
    mutable swarm::url_fetcher m_manager; //@note: Why should I do this mutable to perform const operations?

    impl_t(cocaine::context_t &context, cocaine::io::reactor_t &reactor, const std::string &name) :
        m_log(new logging::log_t(context, name)),
        m_logger_base(m_log),
        m_logger(m_logger_base, blackhole::log::attributes_t()),
        m_loop(reactor.native(), m_logger),
        m_manager(m_loop, m_logger)
    {
    }

    template<typename T, typename H>
    cocaine::deferred<T>
    do_rest_get(const std::string &url, H handler) const {
        Get<T> action(m_manager);
        return do_rest<T, H, Get<T>>(url, handler, action);
    }

    template<typename T, typename H>
    cocaine::deferred<T>
    do_rest_post(const std::string &url, const std::string &body, H handler) const {
        Post<T> action { m_manager, body };
        return do_rest<T, H, Post<T>>(url, handler, action);
    }

    template<typename T, typename H>
    cocaine::deferred<T>
    do_rest_delete(const std::string &url, H handler) const {
#ifdef ELASTICSEARCH_DELETE_SUPPORT
        Delete<T> action { m_manager };
        return do_rest<T, H, Delete<T>>(url, handler, action);
#else
        cocaine::deferred<response::delete_index> deferred;
        deferred.abort(-1, "Delete operation is not supported");
        return deferred;
#endif
    }

    template<typename T, typename H, typename Action>
    cocaine::deferred<T>
    do_rest(const std::string &url, H handler, Action action) const {
        cocaine::deferred<T> deferred;
        request_handler_t<T> request_handler(deferred, handler);

        swarm::url_fetcher::request request;
        request.set_url(url);
        action(request, request_handler);
        return deferred;
    }
};

elasticsearch_t::elasticsearch_t(cocaine::context_t &context, cocaine::io::reactor_t &reactor, const std::string &name, const Json::Value &args) :
    service_t(context, reactor, name, args),
    d(new impl_t(context, reactor, name))
{
    const std::string &host = args.get("host", "127.0.0.1").asString();
    const uint16_t port = args.get("port", 9200).asUInt();
    d->m_url_prefix = cocaine::format("http://%s:%d", host, port);

    COCAINE_LOG_INFO(d->m_log, "Elasticsearch endpoint: %s", d->m_url_prefix);

    on<io::elasticsearch::get>("get", std::bind(&elasticsearch_t::get, this, _1, _2, _3));
    on<io::elasticsearch::index>("index", std::bind(&elasticsearch_t::index, this, _1, _2, _3, _4));
    on<io::elasticsearch::search>("search", std::bind(&elasticsearch_t::search, this, _1, _2, _3, _4));
    on<io::elasticsearch::delete_index>("delete", std::bind(&elasticsearch_t::delete_index, this, _1, _2, _3));
}

elasticsearch_t::~elasticsearch_t() {
}

cocaine::deferred<response::get>
elasticsearch_t::get(const std::string &index, const std::string &type, const std::string &id) const {
    const std::string &url = cocaine::format("%s/%s/%s/%s/", d->m_url_prefix, index, type, id);
    get_handler_t handler { d->m_log };
    return d->do_rest_get<response::get>(url, handler);
}

cocaine::deferred<response::index>
elasticsearch_t::index(const std::string &data, const std::string &index, const std::string &type, const std::string &id) const {
    const std::string &url = cocaine::format("%s/%s/%s/%s", d->m_url_prefix, index, type, id);
    index_handler_t handler { d->m_log };
    return d->do_rest_post<response::index>(url, data, handler);
}

cocaine::deferred<response::search>
elasticsearch_t::search(const std::string &index, const std::string &type, const std::string &query, int size) const {
    if (size <= 0)
        throw cocaine::error_t("desired search size (%d) must be positive number", size);

    const std::string &url = cocaine::format("%s/%s/%s/_search?q=%s&size=%d", d->m_url_prefix, index, type, query, size);
    search_handler_t handler { d->m_log };
    return d->do_rest_get<response::search>(url, handler);
}

cocaine::deferred<response::delete_index>
elasticsearch_t::delete_index(const std::string &index, const std::string &type, const std::string &id) const {
    const std::string &url = cocaine::format("%s/%s/%s/%s", d->m_url_prefix, index, type, id);
    delete_handler_t handler { d->m_log };
    return d->do_rest_delete<response::delete_index>(url, handler);
}
