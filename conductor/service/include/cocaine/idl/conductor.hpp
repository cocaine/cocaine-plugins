/*
* 2015+ Copyright (c) Dmitry Unkovsky <diunko@yandex-team.ru>
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

#ifndef COCAINE_CONDUCTOR_SERVICE_INTERFACE_HPP
#define COCAINE_CONDUCTOR_SERVICE_INTERFACE_HPP

#include <cocaine/rpc/protocol.hpp>
#include <cocaine/idl/primitive.hpp>

#include <boost/mpl/list.hpp>
#include <vector>

namespace cocaine { namespace io {

struct conductor_tag;

struct conductor {

    struct subscribe {
        typedef conductor_tag tag;

        static const char* alias() {
            return "subscribe";
        }

        typedef boost::mpl::list<
            uint64_t // client_id
        >::type argument_type;

        typedef stream_of<
            uint64_t, // request id
            std::map<std::string, std::map<std::string, std::string>>  // request itself
        >::tag upstream_type;
    };


    struct spool_done {
        typedef conductor_tag tag;

        static const char* alias() {
            return "spool_done";
        }

        typedef boost::mpl::list<
            uint64_t, // request_id
            std::error_code, // code, if any
            optional<std::string> // error message, if any
            //std::string // error message, if any
        > argument_type;
    };

    struct spawn_done {
        typedef conductor_tag tag;

        static const char* alias() {
            return "spawn_done";
        }

        typedef boost::mpl::list<
            uint64_t, // request_id
            std::error_code, // code, if any
            optional<std::string> // error message, if any
            //std::string // error message, if any
        > argument_type;
    };
};

template<>
struct protocol<conductor_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        conductor::subscribe,
        conductor::spool_done,
        conductor::spawn_done
    > messages;

    typedef conductor type;
};

}} // namespace cocaine::io

#endif
