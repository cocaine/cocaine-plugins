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

#include <string>

#include <boost/mpl/list.hpp>
#include <boost/optional.hpp>

#include <cocaine/rpc/tags.hpp>

namespace cocaine { namespace io {

struct elasticsearch_tag;

namespace elasticsearch {

struct get {
    typedef elasticsearch_tag tag;

    typedef boost::mpl::list<
        /* index */ std::string,
        /* type */ std::string,
        /* id */ std::string
    > tuple_type;

    typedef boost::mpl::list<
        /* status */ bool,
        /* response */ std::string
    >result_type;
};

struct index {
    typedef elasticsearch_tag tag;

    typedef boost::mpl::list<
        /* data */ std::string,
        /* index */ std::string,
        /* type */ std::string,
        /* id */ optional<std::string>
    > tuple_type;

    typedef boost::mpl::list<
        /* status */ bool,
        /* id */ std::string
    >result_type;
};

struct search {
    typedef elasticsearch_tag tag;

    typedef boost::mpl::list<
        /* index */ std::string,
        /* type */ std::string,
        /* query */ std::string,
        /* size */ optional_with_default<int, 10>
    > tuple_type;

    typedef boost::mpl::list<
        /* status */ bool,
        /* count */ int,
        /* id */ std::string
    > result_type;
};

struct delete_index {
    typedef elasticsearch_tag tag;

    typedef boost::mpl::list<
        /* index */ std::string,
        /* type */ std::string,
        /* id */ std::string
    > tuple_type;

    typedef bool result_type;
};

}

template<>
struct protocol<elasticsearch_tag> {
    typedef mpl::list<
        elasticsearch::get,
        elasticsearch::index,
        elasticsearch::search,
        elasticsearch::delete_index
    > type;

    typedef boost::mpl::int_<
        1
    >::type version;
};

} }
