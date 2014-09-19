/*
    Copyright (c) 2011-2013 Evgeny Safronov <esafronov@yandex-team.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <cocaine/rpc/protocol.hpp>

namespace cocaine { namespace io {

struct elasticsearch_tag;

struct elasticsearch {
    struct get {
        typedef elasticsearch_tag tag;

        static const char* alias() {
            return "get";
        }

        typedef boost::mpl::list<
            /* index */ std::string,
            /* type */ std::string,
            /* id */ std::string
        > tuple_type;

        typedef stream_of<
            /* status */ bool,
            /* response */ std::string
        >::tag upstream_type;
    };

    struct index {
        typedef elasticsearch_tag tag;

        static const char* alias() {
            return "index";
        }

        typedef boost::mpl::list<
            /* data */ std::string,
            /* index */ std::string,
            /* type */ std::string,
            /* id */ optional<std::string>
        > tuple_type;

        typedef stream_of<
            /* status */ bool,
            /* id */ std::string
        >::tag upstream_type;
    };

    struct search {
        typedef elasticsearch_tag tag;

        static const char* alias() {
            return "search";
        }

        static const int DEFAULT_SIZE = 10;

        typedef boost::mpl::list<
            /* index */ std::string,
            /* type */ std::string,
            /* query */ std::string,
            /* size */ optional_with_default<int, DEFAULT_SIZE>
        > tuple_type;

        typedef stream_of<
            /* status */ bool,
            /* count */ int,
            /* id */ std::string
        >::tag upstream_type;
    };

    struct delete_index {
        typedef elasticsearch_tag tag;

        static const char* alias() {
            return "delete";
        }

        typedef boost::mpl::list<
            /* index */ std::string,
            /* type */ std::string,
            /* id */ std::string
        > tuple_type;

        typedef stream_of<
            bool
        >::tag upstream_type;
    };
};

template<>
struct protocol<elasticsearch_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef mpl::list<
        elasticsearch::get,
        elasticsearch::index,
        elasticsearch::search,
        elasticsearch::delete_index
    > messages;

    typedef elasticsearch type;
};

} }
