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

#ifndef COCAINE_GRAPHITE_SERVICE_INTERFACE_HPP
#define COCAINE_GRAPHITE_SERVICE_INTERFACE_HPP

#include "cocaine/metric.hpp"
#include <cocaine/rpc/protocol.hpp>
#include <boost/mpl/list.hpp>
#include <vector>

namespace cocaine { namespace io {

struct graphite_tag;

struct graphite {
    struct send_bulk {
        typedef graphite_tag tag;

        static const char* alias() {
            return "send_bulk";
        }

        typedef boost::mpl::list<
            service::graphite::metric_pack_t //bunch of metrics
        > argument_type;
    };

    struct send_one {
        typedef graphite_tag tag;

        static const char* alias() {
            return "send_one";
        }

        typedef boost::mpl::list<
            service::graphite::metric_t //one metric
        > argument_type;
    };
};

template<>
struct protocol<graphite_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        graphite::send_bulk,
        graphite::send_one
    > messages;

    typedef graphite type;
};

}} // namespace cocaine::io

#endif
