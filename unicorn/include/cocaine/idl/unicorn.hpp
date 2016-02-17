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

#pragma once

#include <cocaine/rpc/protocol.hpp>

#include <cocaine/unicorn/path.hpp>
#include <cocaine/unicorn/value.hpp>

#include <boost/mpl/list.hpp>

#include <vector>

namespace cocaine { namespace io {

struct unicorn_tag;
struct unicorn_final_tag;

/**
* Protocol starts with initial dispatch.
*
* All methods except lock move protocol to unicorn_final_tag,
* which provides only close leading to terminal transition.
* This is done in order to create a dispatch during transition which controls lifetime of the session.
*
* "lock" moves protocol to locked_tag which also controls lifetime of the lock.
* It has only unlock method leading to terminal transition.
*/
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
            cocaine::unicorn::path_t,
            cocaine::unicorn::value_t
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
        **/
        typedef boost::mpl::list<
            cocaine::unicorn::path_t,
            cocaine::unicorn::value_t,
            cocaine::unicorn::version_t
        > argument_type;

        /**
        * Return value is current value in ZK.
        */
        typedef option_of<
            bool,
            cocaine::unicorn::versioned_value_t
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
            cocaine::unicorn::path_t
        > argument_type;

        /**
        * current version in ZK
        */
        typedef option_of<
            cocaine::unicorn::versioned_value_t
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
            cocaine::unicorn::path_t
        > argument_type;

        /**
        * current version in ZK
        */
        typedef stream_of<
            cocaine::unicorn::versioned_value_t
        >::tag upstream_type;

        typedef unicorn_final_tag dispatch_type;
    };

    /**
     * delete node. replaces del method as "del" is a reserved word in Python
     * */
    struct remove{
        typedef unicorn_tag tag;

        static const char* alias() {
            return "remove";
        }

        /**
        * delete node. Will only succeed if there are no child nodes.
        */
        typedef boost::mpl::list<
        cocaine::unicorn::path_t,
        cocaine::unicorn::version_t
        > argument_type;

        typedef option_of<
        bool
        >::tag upstream_type;

        typedef unicorn_final_tag dispatch_type;
    };

    /**
     * Delete node. Deprecated, use remove instead.
     */
    struct del: public remove {
        using remove::tag;
        using remove::argument_type;
        using remove::upstream_type;
        using remove::dispatch_type;
        static const char* alias() {
            return "del";
        }
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
            cocaine::unicorn::path_t,
            cocaine::unicorn::value_t
        > argument_type;

        /**
        * return value after increment
        */
        typedef option_of <
            cocaine::unicorn::versioned_value_t
        >::tag upstream_type;

        typedef unicorn_final_tag dispatch_type;
    };

    struct children_subscribe {
        typedef unicorn_tag tag;

        static const char* alias() {
            return "children_subscribe";
        }

        /**
        * subscribe for updates of children of the node. It will return actual list of children on each child creation/deletion.
        */
        typedef boost::mpl::list<
            cocaine::unicorn::path_t
        > argument_type;

        typedef stream_of<
            cocaine::unicorn::version_t,
            std::vector<std::string>
        >::tag upstream_type;

        typedef unicorn_final_tag dispatch_type;
    };

    struct lock {
        typedef unicorn_tag tag;

        static const char* alias() {
            return "lock";
        }

        typedef unicorn_final_tag dispatch_type;

        typedef boost::mpl::list<
            cocaine::unicorn::path_t
        > argument_type;

        typedef option_of <
            bool
        >::tag upstream_type;
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
        unicorn::subscribe,
        unicorn::children_subscribe,
        unicorn::put,
        unicorn::get,
        unicorn::create,
        unicorn::del,

        // alias for del method
        unicorn::remove,
        unicorn::increment,
        unicorn::lock
    > messages;

    typedef unicorn scope;
};

template<>
struct protocol<unicorn_final_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        unicorn::close
    > messages;

    typedef unicorn scope;
};

}} // namespace cocaine::io

