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

/// Tag type for cache protocol.
struct cache_tag;

/// Represents cache service methods scope.
struct cache {
    /// Represents put method interface.
    struct put {
        typedef cache_tag tag;

        static const char* alias() {
            return "put";
        }

        typedef boost::mpl::list<
            /// Key.
            std::string,
            /// Value.
            std::string
        > argument_type;
    };

    /// Represents get method interface.
    struct get {
        typedef cache_tag tag;

        static const char* alias() {
            return "get";
        }

        typedef boost::mpl::list<
            /// Key.
            std::string
        > argument_type;

        typedef option_of<
            /// Exists flag.
            bool,
            /// Value.
            std::string
        >::tag upstream_type;
    };
};

/// Explicit protocol template specialization for cache service.
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
