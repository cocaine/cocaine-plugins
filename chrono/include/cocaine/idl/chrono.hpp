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

#ifndef COCAINE_CHRONO_SERVICE_INTERFACE_HPP
#define COCAINE_CHRONO_SERVICE_INTERFACE_HPP

#include <cocaine/rpc/protocol.hpp>

namespace cocaine { namespace io {

struct chrono_tag;

typedef int64_t timer_id_t;

struct chrono {
    struct notify_after {
        typedef chrono_tag tag;

        static const char* alias() {
            return "notify_after";
        }

        typedef boost::mpl::list<
            /* time difference */ double,
            /* send id */ optional_with_default<bool, false>
        > argument_type;

        typedef stream_of<
            timer_id_t
        >::tag upstream_type;
    };

    struct notify_every {
        typedef chrono_tag tag;

        static const char* alias() {
            return "notify_every";
        }

        typedef boost::mpl::list<
            /* time difference */ double,
            /* send id */ optional_with_default<bool, false>
        > argument_type;

        typedef stream_of<
            timer_id_t
        >::tag upstream_type;
    };

    struct cancel {
        typedef chrono_tag tag;

        static const char* alias() {
            return "cancel";
        }

        typedef boost::mpl::list<
            /* timer id */ timer_id_t
        > argument_type;
    };

    struct restart {
        typedef chrono_tag tag;

        static const char* alias() {
            return "restart";
        }

        typedef boost::mpl::list<
            /* timer id */ timer_id_t
        > argument_type;
    };
};

template<>
struct protocol<chrono_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef mpl::list<
        chrono::notify_after,
        chrono::notify_every,
        chrono::cancel,
        chrono::restart
    > messages;

    typedef chrono type;
};

}} // namespace cocaine::io

#endif
