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

namespace cocaine { namespace io {

struct unicorn_tag;
struct unicorn_final_tag;
struct unicorn_locked_tag;

using namespace cocaine::unicorn;

struct unicorn {
    struct create {
        typedef unicorn_tag tag;

        static const char* alias() {
            return "create";
        }

        /**
        * Create a node if it does not exist.
        *
        * path_t - path to change.
        * value_t - value to write in path
        **/
        typedef boost::mpl::list<
            path_t,
            value_t
        > argument_type;

        /**
        * true if node was created. Error on any kind of error
        */
        typedef option_of<
            bool
        >::tag upstream_type;

        typedef unicorn_final_tag dispatch_type;
    };

    struct put {
        typedef unicorn_tag tag;

        static const char* alias() {
            return "put";
        }

        /**
        *  put command implements cas behaviour. It accepts:
        *
        * path_t - path to change.
        * value_t - value to write in path
        * version_t - version to compare with. If version in zk do not match - error will be returned.
        *   -1 indicates that version check is not performed (forced put).
        **/
        typedef boost::mpl::list<
            path_t,
            value_t,
            version_t
        > argument_type;

        /**
        * Return value is current value in ZK.
        */
        typedef option_of<
            bool,
            versioned_value_t
        >::tag upstream_type;

        typedef unicorn_final_tag dispatch_type;
    };

    struct get {
        typedef unicorn_tag tag;

        static const char* alias() {
            return "get";
        }

        /**
        * subscribe for updates on path. Will send last update which version is greater than specified.
        */
        typedef boost::mpl::list<
            path_t
        > argument_type;

        /**
        * current version in ZK
        */
        typedef option_of<
            versioned_value_t
        >::tag upstream_type;

        typedef unicorn_final_tag dispatch_type;
    };

    struct subscribe {
        typedef unicorn_tag tag;

        static const char* alias() {
            return "subscribe";
        }

        /**
        * subscribe for updates on path. Will send last update which version is greater than specified.
        */
        typedef boost::mpl::list<
            path_t
        > argument_type;

        /**
        * current version in ZK
        */
        typedef stream_of<
            versioned_value_t
        >::tag upstream_type;

        typedef unicorn_final_tag dispatch_type;
    };

    struct del {
        typedef unicorn_tag tag;

        static const char* alias() {
            return "del";
        }

        /**
        * delete node. Will only succeed if there are no child nodes.
        */
        typedef boost::mpl::list<
            path_t,
            version_t
        > argument_type;

        typedef option_of<
            bool
        >::tag upstream_type;

        typedef unicorn_final_tag dispatch_type;
    };

    struct increment {
        typedef unicorn_tag tag;

        static const char* alias() {
            return "increment";
        }

        /**
        * increment node value in path by passed value.
        * If either passed or stored value is not numeric will return error.
        * If one of the values is float - result value will be float.
        */
        typedef boost::mpl::list<
            path_t,
            value_t
        > argument_type;

        /**
        * return value after increment
        */
        typedef option_of <
            versioned_value_t
        >::tag upstream_type;

        typedef unicorn_final_tag dispatch_type;
    };

    struct lsubscribe {
        typedef unicorn_tag tag;

        static const char* alias() {
            return "lsubscribe";
        }

        typedef boost::mpl::list<
            path_t
        > argument_type;

        typedef stream_of<
            version_t,
            std::vector<std::string>
        >::tag upstream_type;

        typedef unicorn_final_tag dispatch_type;
    };

    struct lock {
        typedef unicorn_tag tag;

        static const char* alias() {
            return "lock";
        }

        typedef unicorn_locked_tag dispatch_type;

        typedef boost::mpl::list<
            path_t
        > argument_type;

        typedef option_of <
            bool
        >::tag upstream_type;
    };

    struct unlock {
        typedef unicorn_locked_tag tag;
        static const char* alias() {
            return "unlock";
        }
    };

    struct close {
        typedef unicorn_final_tag tag;
        static const char* alias() {
            return "close";
        }
    };
};

template<>
struct protocol<unicorn_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        unicorn::get,
        unicorn::subscribe,
        unicorn::lsubscribe,
        unicorn::put,
        unicorn::create,
        unicorn::del,
        unicorn::increment,
        unicorn::lock
    > messages;

    typedef unicorn type;
};

template<>
struct protocol<unicorn_locked_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        unicorn::unlock
    > messages;

    typedef unicorn type;
};

template<>
struct protocol<unicorn_final_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        unicorn::close
    > messages;

    typedef unicorn type;
};

}} // namespace cocaine::io

#endif
