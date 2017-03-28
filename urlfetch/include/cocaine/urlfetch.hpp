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

#include <cocaine/rpc/dispatch.hpp>
#include <cocaine/rpc/tags.hpp>

#include <boost/asio/io_service.hpp>
#include <boost/thread.hpp>

#include <swarm/urlfetcher/url_fetcher.hpp>
#include <swarm/urlfetcher/boost_event_loop.hpp>

#include "cocaine/idl/urlfetch.hpp"

namespace cocaine { namespace service {

class urlfetch_t:
    public api::service_t,
    public dispatch<io::urlfetch_tag>
{
    public:
        typedef result_of<io::urlfetch::get>::type get_result_type;

        urlfetch_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args);
        ~urlfetch_t();

        virtual
        auto
        prototype() -> io::basic_dispatch_t& {
            return *this;
        }

    private:
        void run_service();

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

        ioremap::swarm::url_fetcher::request
        prepare_request(const std::string& url,
                        int timeout,
                        const std::map<std::string, std::string>& cookies,
                        const std::map<std::string, std::string>& headers,
                        bool follow_location);

    private:
        std::shared_ptr<logging::logger_t> log_;
        ioremap::swarm::logger m_logger;
        boost::asio::io_service m_service;
        ioremap::swarm::boost_event_loop m_loop;
        ioremap::swarm::url_fetcher m_manager;
        std::unique_ptr<boost::asio::io_service::work> m_work;
        boost::thread m_thread;
};

}} // namespace cocaine::service

#endif
