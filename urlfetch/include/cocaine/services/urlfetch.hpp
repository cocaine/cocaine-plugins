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
#include <cocaine/asio/reactor.hpp>
#include <cocaine/rpc/tags.hpp>

#include <swarm/networkmanager.h>

namespace cocaine { namespace io {

struct urlfetch_tag;

namespace urlfetch {
    struct get {
        typedef urlfetch_tag tag;

        typedef boost::mpl::list<
            /* url */ std::string,
            /* timeout */ optional_with_default<int, 5000>,
            /* cookies */ optional<std::map<std::string, std::string>>,
            /* headers */ optional<std::map<std::string, std::string>>,
            /* follow_location */ optional_with_default<bool, true>
        > tuple_type;

        typedef boost::mpl::list<
            /* success */ bool,
            /* data */ std::string,
            /* code */ int,
            /* headers */ std::map<std::string, std::string>
        > result_type;
    };

    struct post {
        typedef urlfetch_tag tag;

        typedef boost::mpl::list<
            /* url */ std::string,
            /* body */ std::string,
            /* timeout */ optional_with_default<int, 5000>,
            /* cookies */ optional<std::map<std::string, std::string>>,
            /* headers */ optional<std::map<std::string, std::string>>,
            /* follow_location */ optional_with_default<bool, true>
        > tuple_type;

        typedef get::result_type result_type;
    };
}

template<>
struct protocol<urlfetch_tag> {
    typedef mpl::list<
        urlfetch::get,
        urlfetch::post
    > type;

    typedef boost::mpl::int_<
        1
    >::type version;
};

} // namespace io

namespace service {

class urlfetch_t:
    public api::service_t
{
    public:
        typedef io::event_traits<io::urlfetch::get>::result_type get_tuple;

        urlfetch_t(context_t& context,
                   io::reactor_t& reactor,
                   const std::string& name,
                   const Json::Value& args);

        void initialize() {}

    private:
        deferred<get_tuple>
        get(const std::string& url,
            int timeout,
            const std::map<std::string, std::string>& cookies,
            const std::map<std::string, std::string>& headers,
            bool follow_location);
        deferred<get_tuple>
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
