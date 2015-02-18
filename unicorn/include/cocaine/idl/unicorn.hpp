/*
* 2015+ Copyright (c) Anton Matveenko <antmat@yandex-team.ru>
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

#ifndef COCAINE_UNICORN_SERVICE_INTERFACE_HPP
#define COCAINE_UNICORN_SERVICE_INTERFACE_HPP

#include "cocaine/unicorn/path.hpp"
#include "cocaine/unicorn/value.hpp"

#include <cocaine/rpc/protocol.hpp>
#include <boost/mpl/list.hpp>
#include <vector>
#include <functional>

namespace cocaine { namespace io {

struct unicorn_tag;

using namespace cocaine::unicorn;

struct unicorn {
    struct put {
        typedef unicorn_tag tag;

        static const char* alias() {
            return "put";
        }

        typedef boost::mpl::list<
            path_t,
            value_t,
            version_t
        > argument_type;

        typedef option_of<
            versioned_value_t
        >::tag upstream_type;
    };

    struct subscribe {
        typedef unicorn_tag tag;

        static const char* alias() {
            return "subscribe";
        }

        typedef boost::mpl::list<
            path_t,
            version_t
        > argument_type;

        typedef stream_of<
            versioned_value_t
        >::tag upstream_type;
    };

    struct del {
        typedef unicorn_tag tag;

        static const char* alias() {
            return "del";
        }

        typedef boost::mpl::list<
            path_t,
            version_t
        > argument_type;

        typedef option_of<
            bool
        >::tag upstream_type;
    };

    struct increment {
        typedef unicorn_tag tag;

        static const char* alias() {
            return "increment";
        }

        typedef boost::mpl::list<
            path_t,
            value_t
        > argument_type;

        typedef option_of <
            value_t
        >::tag upstream_type;
    };
};

template<>
struct protocol<unicorn_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        unicorn::subscribe,
        unicorn::put,
        unicorn::del,
        unicorn::increment
    > messages;

    typedef unicorn type;
};

}} // namespace cocaine::io

#endif