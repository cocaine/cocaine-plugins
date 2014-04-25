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

#include "cocaine/urlfetch.hpp"

#include <boost/foreach.hpp>
#include <boost/format.hpp>

#include <cocaine/logging.hpp>
#include <cocaine/traits/tuple.hpp>

#include <swarm/logger.hpp>
#include <swarm/urlfetcher/stream.hpp>

using namespace cocaine;
using namespace cocaine::io;
using namespace cocaine::service;

using namespace ioremap;

class urlfetch_logger_interface : public swarm::logger_interface
{
public:
    urlfetch_logger_interface(const std::shared_ptr<logging::log_t> &log) : log_(log)
    {
    }

    virtual void log(int level, const char *msg)
    {
        logging::priorities verbosity = logging::priorities::debug;
        switch (level) {
        case swarm::SWARM_LOG_DATA:
            verbosity = logging::priorities::ignore;
        case swarm::SWARM_LOG_ERROR:
            verbosity = logging::priorities::error;
        case swarm::SWARM_LOG_INFO:
            verbosity = logging::priorities::info;
        case swarm::SWARM_LOG_NOTICE:
            verbosity = logging::priorities::info;
        case swarm::SWARM_LOG_DEBUG:
        default:
            verbosity = logging::priorities::debug;
        }

        if (log_->verbosity() >= verbosity)
            log_->emit(verbosity, msg);
    }

    virtual void reopen()
    {
    }

private:
    std::shared_ptr<logging::log_t> log_;
};

urlfetch_t::urlfetch_t(context_t& context,
                       reactor_t& reactor,
                       const std::string& name,
                       const dynamic_t& args):
    service_t(context, reactor, name, args),
    implements<io::urlfetch_tag>(context, name),
    log_(new logging::log_t(context, name)),
    m_logger(new urlfetch_logger_interface(log_), swarm::SWARM_LOG_DEBUG),
    m_loop(m_service),
    m_manager(m_loop, m_logger),
    m_work(new boost::asio::io_service::work(m_service))
{
    int connections_limits = args.as_object().at("connections-limit", 10).to<unsigned int>();;
    m_manager.set_total_limit(connections_limits);

    using namespace std::placeholders;

    on<io::urlfetch::get>(std::bind(&urlfetch_t::get, this, _1, _2, _3, _4, _5));
    on<io::urlfetch::post>(std::bind(&urlfetch_t::post, this, _1, _2, _3, _4, _5, _6));

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
    deferred<urlfetch_t::get_result_type> promise;
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

        urlfetch_t::get_result_type tuple = std::make_tuple(success, data, code, headers);
        promise.write(tuple);
    }
};

}

deferred<urlfetch_t::get_result_type>
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

deferred<urlfetch_t::get_result_type>
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
