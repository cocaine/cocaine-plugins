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

#ifndef COCAINE_CACHE_SERVICE_INTERFACE_HPP
#define COCAINE_CACHE_SERVICE_INTERFACE_HPP

#include <cocaine/rpc/protocol.hpp>

namespace cocaine { namespace io {

struct cache_tag;

struct cache {
    struct put {
        typedef cache_tag tag;

        static const char* alias() {
            return "put";
        }

        typedef boost::mpl::list<
            /* key */ std::string,
            /* value */ std::string
        > argument_type;
    };

    struct get {
        typedef cache_tag tag;

        static const char* alias() {
            return "get";
        }

        typedef boost::mpl::list<
            /* key */ std::string
        > argument_type;

        typedef stream_of<
            /* exists */ bool,
            /* value */ std::string
        >::tag upstream_type;
    };
};

template<>
struct protocol<cache_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef mpl::list<
        cache::get,
        cache::put
    > messages;

    typedef cache type;
};

}} // namespace cocaine::io

#endif
