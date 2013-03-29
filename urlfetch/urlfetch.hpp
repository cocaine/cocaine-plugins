/*
* 2013+ Copyright (c) Ruslan Nigatullin <euroelessar@yandex.ru>
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

#include <swarm/networkmanager.h>

namespace cocaine {

namespace service {

using io::reactor_t;

typedef ioremap::swarm::network_request network_request_t;
typedef ioremap::swarm::network_reply network_reply_t;

class urlfetch_t:
    public api::service_t
{
    public:
        urlfetch_t(context_t& context,
                   reactor_t& reactor,
                   const std::string& name,
                   const Json::Value& args);

    private:
        deferred<network_reply_t>
        get(const std::string& url,
            const std::map<std::string, std::string>& headers,
            bool recurse);

    private:
        ioremap::swarm::network_manager m_manager;
};

} // namespace service

namespace io {

struct urlfetch_tag;

namespace urlfetch {
    struct get {
        typedef urlfetch_tag tag;

        typedef boost::mpl::list<
            /* url */     std::string,
            /* headers */ std::map<std::string, std::string>,
            /* recurse */ bool
        > tuple_type;
    };
}

template<>
struct type_traits<service::network_reply_t>
{
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer,
         const service::network_reply_t& source)
    {
        packer.pack_array(5);

        packer << source.url;
        packer << source.headers;
        packer << source.code;
        packer << source.error;
        packer << source.data;
    }
};

template<>
struct protocol<urlfetch_tag> {
    typedef mpl::list<
        urlfetch::get
    > type;
};

} // namespace io

} // namespace cocaine

#endif
