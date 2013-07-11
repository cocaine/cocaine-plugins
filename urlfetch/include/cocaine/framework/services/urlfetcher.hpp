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

namespace cocaine { namespace framework {

class urlfetcher_service_t:
    public service_t
{
    public:
        static const unsigned int version = cocaine::io::protocol<cocaine::io::urlfetch_tag>::version::value;

        urlfetcher_service_t(std::shared_ptr<service_connection_t> connection) :
            service_t(connection)
        { }

        service_traits<cocaine::io::urlfetch::get>::future_type
        get(const std::string& url) {
            return call<io::urlfetch::get>(url);
        }

        service_traits<cocaine::io::urlfetch::get>::future_type
        get(const std::string& url, int timeout) {
            return call<io::urlfetch::get>(url, timeout);
        }

        service_traits<cocaine::io::urlfetch::get>::future_type
        get(const std::string& url,
            int timeout,
            const std::map<std::string, std::string>& cookies)
        {
            return call<io::urlfetch::get>(url, timeout, cookies);
        }

        service_traits<cocaine::io::urlfetch::get>::future_type
        get(const std::string& url,
            int timeout,
            const std::map<std::string, std::string>& cookies,
            const std::map<std::string, std::string>& headers)
        {
            return call<io::urlfetch::get>(url, timeout, cookies, headers);
        }

        service_traits<cocaine::io::urlfetch::get>::future_type
        get(const std::string& url,
            int timeout,
            const std::map<std::string, std::string>& cookies,
            const std::map<std::string, std::string>& headers,
            bool follow_location)
        {
            return call<io::urlfetch::get>(url, timeout, cookies, headers, follow_location);
        }

        service_traits<cocaine::io::urlfetch::post>::future_type
        post(const std::string& url, const std::string& body) {
            return call<io::urlfetch::post>(url, body);
        }

        service_traits<cocaine::io::urlfetch::post>::future_type
        post(const std::string& url, const std::string& body, int timeout) {
            return call<io::urlfetch::post>(url, body, timeout);
        }

        service_traits<cocaine::io::urlfetch::post>::future_type
        post(const std::string& url,
             const std::string& body,
             int timeout,
             const std::map<std::string, std::string>& cookies)
        {
            return call<io::urlfetch::post>(url, body, timeout, cookies);
        }

        service_traits<cocaine::io::urlfetch::post>::future_type
        post(const std::string& url,
             const std::string& body,
             int timeout,
             const std::map<std::string, std::string>& cookies,
             const std::map<std::string, std::string>& headers)
        {
            return call<io::urlfetch::post>(url, body, timeout, cookies, headers);
        }

        service_traits<cocaine::io::urlfetch::post>::future_type
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
