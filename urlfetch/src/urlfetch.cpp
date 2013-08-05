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
#include <cocaine/traits/tuple.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <swarm/logger.h>

using namespace cocaine;
using namespace cocaine::io;
using namespace cocaine::service;

using namespace ioremap;

using namespace std::placeholders;

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
        case swarm::LOG_DATA:
            verbosity = logging::priorities::ignore;
        case swarm::LOG_ERROR:
            verbosity = logging::priorities::error;
        case swarm::LOG_INFO:
            verbosity = logging::priorities::info;
        case swarm::LOG_NOTICE:
            verbosity = logging::priorities::info;
        case swarm::LOG_DEBUG:
        default:
            verbosity = logging::priorities::debug;
        }

        if (log_->verbosity() >= verbosity)
            log_->emit(verbosity, msg);
    }

private:
    std::shared_ptr<logging::log_t> log_;
};

urlfetch_t::urlfetch_t(context_t& context,
                       reactor_t& reactor,
                       const std::string& name,
                       const Json::Value& args):
    service_t(context, reactor, name, args),
    m_logger(new urlfetch_logger_interface(log_), swarm::LOG_DATA),
    m_manager(reactor.native(), m_logger),
    log_(new logging::log_t(context, name))
{
    on<io::urlfetch::get>("get", std::bind(&urlfetch_t::get, this, _1, _2, _3, _4, _5));
    on<io::urlfetch::post>("post", std::bind(&urlfetch_t::post, this, _1, _2, _3, _4, _5, _6));
}

namespace {

struct urlfetch_get_handler {
    deferred<urlfetch_t::get_tuple> promise;
    std::shared_ptr<logging::log_t> log_;

    void
    operator()(const swarm::network_reply& reply) {
        const std::string data = reply.get_data();
        const int code = reply.get_code();
        bool success = (reply.get_error() == 0 && (code < 400 || code >= 600) );

        if (success) {
            COCAINE_LOG_DEBUG(log_, "Downloaded successfully %s, http code %d", reply.get_url(), reply.get_code() );
        } else {
            COCAINE_LOG_DEBUG(log_, "Unable to download %s, network error code %d, http code %d", reply.get_url(), reply.get_error(), reply.get_code() );

            if (reply.get_code() == 0) {
                // Socket-only error, no valid http response
                promise.abort(-reply.get_error(),
                              cocaine::format("Unable to download %s, network error code %d",
                                              reply.get_request().get_url(),
                                              reply.get_error()));
                return;
            }
        }

        std::map<std::string, std::string> headers;

        BOOST_FOREACH(const auto& it, reply.get_headers()) {
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

    m_manager.get(handler,
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

    m_manager.post(handler,
                   prepare_request(url,
                                   timeout,
                                   cookies,
                                   headers,
                                   follow_location),
                   body);

    return handler.promise;
}

swarm::network_request
urlfetch_t::prepare_request(const std::string& url,
                            int timeout,
                            const std::map<std::string, std::string>& cookies,
                            const std::map<std::string, std::string>& headers,
                            bool follow_location)
{
    swarm::network_request request;

    request.set_url(url);
    request.set_follow_location(follow_location);
    request.set_timeout(timeout);

    COCAINE_LOG_DEBUG(log_, "Downloading %s", url);

    BOOST_FOREACH(const auto& it, headers) {
        const auto& header_name = it.first;
        const auto& header_value = it.second;

        request.add_header(header_name, header_value);
    }

    BOOST_FOREACH(const auto& it, cookies) {
        const auto& cookie_name = it.first;
        const auto& cookie_value = it.second;

        std::string cookie_header = boost::str(boost::format("%1%=%2%") % cookie_name % cookie_value);

        request.add_header("Cookie", cookie_header);
    }

    return request;
}
