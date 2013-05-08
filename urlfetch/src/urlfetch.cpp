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
#include <boost/foreach.hpp>
#include <boost/format.hpp>

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
    m_manager(reactor.native()),
    log_(new logging::log_t(context, name))
{
    on<io::urlfetch::get>("get", std::bind(&urlfetch_t::get, this, _1, _2, _3, _4, _5));
}

namespace {
    struct urlfetch_get_handler {
        deferred<get_tuple> promise;
		std::shared_ptr<logging::log_t> log_;
		
        void
        operator()(const network_reply_t& reply) {
			std::string data = reply.data;
			int code = reply.code;
			bool success = (reply.error == 0 && (code < 400 || code >= 600) );
			
			if (success) {
				COCAINE_LOG_DEBUG(log_, "Downloaded successfully %s, http code %d", reply.url, reply.code );
			} else {
				COCAINE_LOG_INFO(log_, "Unable to download %s , network error code %d , http code %d", reply.url, reply.error, reply.code );
			}
			
			std::map<std::string, std::string> headers;
			
			BOOST_FOREACH(const auto& it, reply.headers) {
				const auto& header_name = it.first;
				const auto& header_value = it.second;
				headers[header_name] = header_value;
			}
			
			get_tuple tuple = std::make_tuple(success, data, code, headers);
			promise.write(tuple);
        }
    };
}

deferred<get_tuple>
urlfetch_t::get(const std::string& url,
                int timeout,
                const std::map<std::string, std::string>& cookies,
                const std::map<std::string, std::string>& headers,
                bool follow_location)
{
    network_request_t request;

    request.url = url;
    request.follow_location = follow_location;

	COCAINE_LOG_DEBUG(log_, "Downloading %s", url);
	
    std::copy(
        headers.begin(),
        headers.end(),
        std::back_inserter(request.headers)
    );
	
	BOOST_FOREACH(const auto& it, cookies) {
		const auto& cookie_name = it.first;
		const auto& cookie_value = it.second;
		
		std::string cookie_header = boost::str(boost::format("%1%=%2%") % cookie_name % cookie_value);
		
		request.headers.push_back(
			std::pair<std::string, std::string>("Cookie", cookie_header));
	}

    urlfetch_get_handler handler;
	handler.log_ = log_;

    m_manager.get(handler, request);

    return handler.promise;
}
