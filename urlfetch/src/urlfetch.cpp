/*
* 2013+ Copyright (c) Ruslan Nigmatullin <euroelessar@yandex.ru>
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

#include "cocaine/services/urlfetch.hpp"
#include <cocaine/logging.hpp>
#include <cocaine/memory.hpp>
#include <cocaine/traits/tuple.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <swarm/urlfetcher/stream.hpp>

#define BOOST_BIND_NO_PLACEHOLDERS
#include <blackhole/formatter/string.hpp>
#undef BOOST_BIND_NO_PLACEHOLDERS

using namespace cocaine;
using namespace cocaine::io;
using namespace cocaine::service;

using namespace ioremap;

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

urlfetch_t::urlfetch_t(context_t& context,
                       reactor_t& reactor,
                       const std::string& name,
                       const Json::Value& args):
    service_t(context, reactor, name, args),
    log_(new logging::log_t(context, name)),
    m_logger_base(new log_adapter_t(log_)),
    m_logger(*m_logger_base, blackhole::log::attributes_t()),
    m_loop(m_service, m_logger),
    m_manager(m_loop, m_logger),
    m_work(new boost::asio::io_service::work(m_service))
{
    int connections_limits = args.get("connections-limit", 10).asInt();
    m_manager.set_total_limit(connections_limits);

    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    on<io::urlfetch::get>("get", std::bind(&urlfetch_t::get, this, _1, _2, _3, _4, _5));
    on<io::urlfetch::post>("post", std::bind(&urlfetch_t::post, this, _1, _2, _3, _4, _5, _6));

    m_thread = boost::thread(std::bind(&urlfetch_t::run_service, this));
}

urlfetch_t::~urlfetch_t()
{
    m_work.reset();
    m_thread.join();
}

void urlfetch_t::run_service()
{
    boost::system::error_code error;
    m_service.run(error);

    if (error) {
        COCAINE_LOG_DEBUG(log_, "Can not run io_service, error %s", error.message() );
    }
}

namespace {

struct urlfetch_get_handler {
    deferred<urlfetch_t::get_tuple> promise;
    std::shared_ptr<logging::log_t> log_;

    void
    operator()(const swarm::url_fetcher::response& reply,
               const std::string& data,
               const boost::system::error_code& error) {
        const int code = reply.code();
        bool success = (!error && (code < 400 || code >= 600) );

        if (success) {
            COCAINE_LOG_DEBUG(log_, "Downloaded successfully %s, http code %d", reply.url().to_string(), reply.code() );
        } else {
            COCAINE_LOG_DEBUG(log_, "Unable to download %s, error %s, http code %d", reply.url().to_string(), error.message(), reply.code() );

            if (reply.code() == 0) {
                // Socket-only error, no valid http response
                promise.abort(error.value(),
                              cocaine::format("Unable to download %s, error %s",
                                              reply.request().url().to_string(),
                                              error.message()));
                return;
            }
        }

        std::map<std::string, std::string> headers;

        BOOST_FOREACH(const auto& it, reply.headers().all()) {
            const auto& header_name = it.first;
            const auto& header_value = it.second;
            headers[header_name] = header_value;
        }

        urlfetch_t::get_tuple tuple = std::make_tuple(success, data, code, headers);
        promise.write(tuple);
    }
};

}

deferred<urlfetch_t::get_tuple>
urlfetch_t::get(const std::string& url,
                int timeout,
                const std::map<std::string, std::string>& cookies,
                const std::map<std::string, std::string>& headers,
                bool follow_location)
{
    urlfetch_get_handler handler;
    handler.log_ = log_;

    m_manager.get(swarm::simple_stream::create(handler),
                  prepare_request(url,
                                  timeout,
                                  cookies,
                                  headers,
                                  follow_location));

    return handler.promise;
}

deferred<urlfetch_t::get_tuple>
urlfetch_t::post(const std::string& url,
                 const std::string& body,
                 int timeout,
                 const std::map<std::string, std::string>& cookies,
                 const std::map<std::string, std::string>& headers,
                 bool follow_location)
{
    urlfetch_get_handler handler;
    handler.log_ = log_;

    m_manager.post(swarm::simple_stream::create(handler),
                   prepare_request(url,
                                   timeout,
                                   cookies,
                                   headers,
                                   follow_location),
                   std::string(body));

    return handler.promise;
}

swarm::url_fetcher::request
urlfetch_t::prepare_request(const std::string& url,
                            int timeout,
                            const std::map<std::string, std::string>& cookies,
                            const std::map<std::string, std::string>& headers,
                            bool follow_location)
{
    swarm::url_fetcher::request request;
    swarm::http_headers &request_headers = request.headers();

    request.set_url(url);
    request.set_follow_location(follow_location);
    request.set_timeout(timeout);

    COCAINE_LOG_DEBUG(log_, "Downloading %s", url);

    BOOST_FOREACH(const auto& it, headers) {
        const auto& header_name = it.first;
        const auto& header_value = it.second;

        request_headers.add(header_name, header_value);
    }

    BOOST_FOREACH(const auto& it, cookies) {
        const auto& cookie_name = it.first;
        const auto& cookie_value = it.second;

        std::string cookie_header = boost::str(boost::format("%1%=%2%") % cookie_name % cookie_value);

        request_headers.add("Cookie", cookie_header);
    }

    return request;
}
