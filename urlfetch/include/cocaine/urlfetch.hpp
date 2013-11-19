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

#ifndef COCAINE_URLFETCH_SERVICE_HPP
#define COCAINE_URLFETCH_SERVICE_HPP

#include <cocaine/api/service.hpp>
#include <cocaine/dispatch.hpp>
#include <cocaine/rpc/result_of.hpp>

#include <swarm/networkmanager.h>

#include "cocaine/idl/urlfetch.hpp"

namespace cocaine { namespace service {

class urlfetch_t:
    public api::service_t,
    public implements<io::urlfetch_tag>
{
    public:
        typedef result_of<io::urlfetch::get>::type get_result_type;

        urlfetch_t(context_t& context, io::reactor_t& reactor, const std::string& name, const dynamic_t& args);

        virtual
        auto
        prototype() -> dispatch_t& {
            return *this;
        }

    private:
        deferred<get_result_type>
        get(const std::string& url,
            int timeout,
            const std::map<std::string, std::string>& cookies,
            const std::map<std::string, std::string>& headers,
            bool follow_location);

        deferred<get_result_type>
        post(const std::string& url,
             const std::string& body,
             int timeout,
             const std::map<std::string, std::string>& cookies,
             const std::map<std::string, std::string>& headers,
             bool follow_location);

        ioremap::swarm::network_request
        prepare_request(const std::string& url,
                        int timeout,
                        const std::map<std::string, std::string>& cookies,
                        const std::map<std::string, std::string>& headers,
                        bool follow_location);

    private:
        std::shared_ptr<logging::log_t> log_;
        ioremap::swarm::logger m_logger;
        ioremap::swarm::network_manager m_manager;
};

}} // namespace cocaine::service

#endif
