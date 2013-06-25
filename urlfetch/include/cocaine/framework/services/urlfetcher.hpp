/*
* 2013+ Copyright (c) Alexander Ponomarev <noname@yandex-team.ru>
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

#ifndef _URLFETCHER_HPP_INCLUDED_
#define _URLFETCHER_HPP_INCLUDED_

#include <cocaine/framework/service.hpp>
#include <cocaine/services/urlfetch.hpp>
#include <cocaine/logging.hpp>

namespace cocaine { namespace framework {

typedef cocaine::io::protocol<cocaine::io::urlfetch_tag>::version version_type;

class urlfetcher_service_t:
    public service_t
{
    public:
        urlfetcher_service_t(const std::string& name,
                             cocaine::io::reactor_t& service,
                             const cocaine::io::tcp::endpoint& resolver,
                             std::shared_ptr<logger_t> logger) :
            service_t(name, service, resolver, logger, version_type())
        { }

        service_t::handler<io::urlfetch::get>::future
        get(const std::string& url) {
            return call<io::urlfetch::get>(url);
        }

        service_t::handler<io::urlfetch::get>::future
        get(const std::string& url, int timeout) {
            return call<io::urlfetch::get>(url, timeout);
        }

        service_t::handler<io::urlfetch::get>::future
        get(const std::string& url,
            int timeout,
            const std::map<std::string, std::string>& cookies)
        {
            return call<io::urlfetch::get>(url, timeout, cookies);
        }

        service_t::handler<io::urlfetch::get>::future
        get(const std::string& url,
            int timeout,
            const std::map<std::string, std::string>& cookies,
            const std::map<std::string, std::string>& headers)
        {
            return call<io::urlfetch::get>(url, timeout, cookies, headers);
        }

        service_t::handler<io::urlfetch::get>::future
        get(const std::string& url,
            int timeout,
            const std::map<std::string, std::string>& cookies,
            const std::map<std::string, std::string>& headers,
            bool follow_location)
        {
            return call<io::urlfetch::get>(url, timeout, cookies, headers, follow_location);
        }

        service_t::handler<io::urlfetch::post>::future
        post(const std::string& url, const std::string& body) {
            return call<io::urlfetch::post>(url, body);
        }

        service_t::handler<io::urlfetch::post>::future
        post(const std::string& url, const std::string& body, int timeout) {
            return call<io::urlfetch::post>(url, body, timeout);
        }

        service_t::handler<io::urlfetch::post>::future
        post(const std::string& url,
             const std::string& body,
             int timeout,
             const std::map<std::string, std::string>& cookies)
        {
            return call<io::urlfetch::post>(url, body, timeout, cookies);
        }

        service_t::handler<io::urlfetch::post>::future
        post(const std::string& url,
             const std::string& body,
             int timeout,
             const std::map<std::string, std::string>& cookies,
             const std::map<std::string, std::string>& headers)
        {
            return call<io::urlfetch::post>(url, body, timeout, cookies, headers);
        }

        service_t::handler<io::urlfetch::post>::future
        post(const std::string& url,
             const std::string& body,
             int timeout,
             const std::map<std::string, std::string>& cookies,
             const std::map<std::string, std::string>& headers,
             bool follow_location)
        {
            return call<io::urlfetch::post>(url, body, timeout, cookies, headers, follow_location);
        }
};

}} // namespace cocaine::framework

#endif /* _URLFETCHER_HPP_INCLUDED_ */
