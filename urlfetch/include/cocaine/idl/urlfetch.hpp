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

#ifndef COCAINE_URLFETCH_SERVICE_INTERFACE_HPP
#define COCAINE_URLFETCH_SERVICE_INTERFACE_HPP

#include <cocaine/rpc/protocol.hpp>

namespace cocaine { namespace io {

struct urlfetch_tag;

struct urlfetch {
    struct get {
        typedef urlfetch_tag tag;

        static const char* alias() {
            return "get";
        }

        typedef boost::mpl::list<
            /* url */ std::string,
            /* timeout */ optional_with_default<int, 5000>,
            /* cookies */ optional<std::map<std::string, std::string>>,
            /* headers */ optional<std::map<std::string, std::string>>,
            /* follow_location */ optional_with_default<bool, true>
        > argument_type;

        typedef option_of<
            /* success */ bool,
            /* data */ std::string,
            /* code */ int,
            /* headers */ std::map<std::string, std::string>
        >::tag upstream_type;
    };

    struct post {
        typedef urlfetch_tag tag;

        static const char* alias() {
            return "post";
        }

        typedef boost::mpl::list<
            /* url */ std::string,
            /* body */ std::string,
            /* timeout */ optional_with_default<int, 5000>,
            /* cookies */ optional<std::map<std::string, std::string>>,
            /* headers */ optional<std::map<std::string, std::string>>,
            /* follow_location */ optional_with_default<bool, true>
        > argument_type;

        typedef get::upstream_type upstream_type;
    };
};

template<>
struct protocol<urlfetch_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef mpl::list<
        urlfetch::get,
        urlfetch::post
    > messages;

    typedef urlfetch type;
};

}} // namespace cocaine::io

#endif
