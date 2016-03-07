/*
    Copyright (c) 2011-2015 Anton Matveenko <antmat@me.com>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/logging/filter.hpp"
#include "cocaine/logging/attribute.hpp"

#include <cocaine/rpc/protocol.hpp>

#include <boost/mpl/list.hpp>

#include <blackhole/attribute.hpp>

namespace cocaine { namespace io {

struct base_log_tag;
struct named_log_tag;

// Logging service interface

struct base_log {
    struct get {
        typedef base_log_tag tag;

        static const char* alias() {
            return "get";
        }

        typedef boost::mpl::list<
            // Name used for log routing and filtering.
            std::string
        >::type argument_type;

        typedef named_log_tag dispatch_type;
    };

    struct list_loggers {
        typedef base_log_tag tag;

        static const char* alias() {
            return "list_loggers";
        }

        typedef option_of<
            std::vector<std::string>
        >::tag upstream_type;
    };

    struct set_filter {
        typedef base_log_tag tag;

        static const char* alias() {
            return "set_filter";
        }

        typedef boost::mpl::list<
            // Logger name
            std::string,
            // Filter itself
            logging::filter_t,
            // TTL of the filter
            filter_t::seconds_t
        >::type argument_type;

        typedef option_of<
        // Id of the filter created.
            logging::filter_t::id_type
        >::tag upstream_type;
    };

    struct remove_filter {
        typedef base_log_tag tag;

        static const char* alias() {
            return "remove_filter";
        }

        typedef boost::mpl::list<
            logging::filter_t::id_type
        >::type argument_type;

        // true if filter was deleted,
        // false if it didn't exist,
        // error in other cases
        typedef option_of<
            bool
        >::tag upstream_type;
    };

    struct list_filters {
        typedef base_log_tag tag;

        static const char* alias() {
            return "list_filters";
        }

        typedef option_of<
            std::vector<std::tuple<
                std::string,
                logging::filter_t::representation_t,
                logging::filter_t::id_type,
                logging::filter_t::disposition_t
            >>
        >::tag upstream_type;
    };


    struct set_cluster_filter {
        typedef base_log_tag tag;

        static const char* alias() {
            return "set_cluster_filter";
        }

        typedef boost::mpl::list<
            // Logger name
            std::string,
            // Filter itself
            logging::filter_t,
            // Time to store filter
            uint64_t
        >::type argument_type;

        typedef option_of<
            // Id of the filter created.
            logging::filter_t::id_type
        >::tag upstream_type;
    };
};

struct named_log {
    struct emit {
        typedef named_log_tag tag;

        static const char* alias() {
            return "emit";
        }

        typedef boost::mpl::list<
         /* Log message. Some meaningful string, with no explicit limits on its length, although
            underlying loggers might silently truncate it. */
            std::string,
        /* Log severity*/
            unsigned int,
         /* Log event attached attributes. */
            optional<logging::attributes_t>
        >::type argument_type;

        typedef void upstream_type;

        typedef named_log_tag dispatch_type;
    };

    struct emit_ack {
        typedef named_log_tag tag;

        static const char* alias() {
            return "emit_ack";
        }

        typedef boost::mpl::list<
        /* Log message. Some meaningful string, with no explicit limits on its length, although
           underlying loggers might silently truncate it. */
            std::string,
        /* Log severity*/
            optional_with_default<unsigned int, 1u>,
        /* Log event attached attributes. */
            optional<logging::attributes_t>
        >::type argument_type;

        typedef option_of<bool>::tag upstream_type;

        typedef named_log_tag dispatch_type;
    };
};

template<>
struct protocol<base_log_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        base_log::get,
        base_log::list_loggers,
        base_log::set_filter,
        base_log::remove_filter,
        base_log::list_filters,
        base_log::set_cluster_filter
    >::type messages;

    typedef base_log scope;
};

template<>
struct protocol<named_log_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        named_log::emit,
        named_log::emit_ack
    >::type messages;

    typedef named_log scope;
};

}} // namespace cocaine::io

