/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_NODE_SERVICE_ISOLATE_IDL_HPP
#define COCAINE_NODE_SERVICE_ISOLATE_IDL_HPP

#include <cocaine/forwards.hpp>

#include <cocaine/rpc/protocol.hpp>

#include <map>
#include <string>

namespace cocaine { namespace io {

struct isolate_tag;
struct isolate_spawned_tag;
struct isolate_spooled_tag;

struct isolate {
    struct spool {
        typedef isolate_tag tag;

        static const char* alias() {
            return "spool";
        }

        typedef boost::mpl::list<
            dynamic_t,
            std::string
        > argument_type;

        typedef isolate_spooled_tag dispatch_type;

        typedef option_of<>::tag upstream_type;
    };

    struct spawn {
        typedef isolate_tag tag;

        static const char* alias() {
            return "spawn";
        }

        typedef boost::mpl::list<
            // Profile args
            dynamic_t,
            // Name
            std::string,
            // Executable
            std::string,
            // Worker args
            std::map<std::string, std::string>,
            // Env
            std::map<std::string, std::string>
        > argument_type;

        typedef isolate_spawned_tag dispatch_type;

        typedef stream_of<
            // stdout/stderr
            std::string
        >::tag upstream_type;
    };

};

struct isolate_spawned {
    struct kill {
        typedef isolate_spawned_tag tag;
        typedef void upstream_type;

        static const char* alias() {
            return "kill";
        }

    };
};

struct isolate_spooled {
    struct cancel {
        typedef isolate_spooled_tag tag;
        typedef void upstream_type;

        static const char* alias() {
            return "cancel";
        }

    };
};

template<>
struct protocol<isolate_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef mpl::list<
        isolate::spool,
        isolate::spawn
    > messages;

    typedef isolate scope;
};

template<>
struct protocol<isolate_spawned_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef mpl::list<
        isolate_spawned::kill
    > messages;

    typedef isolate_spawned scope;
};

template<>
struct protocol<isolate_spooled_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef mpl::list<
        isolate_spooled::cancel
    > messages;

    typedef isolate_spawned scope;
};


}} // namespace cocaine::io
#endif
