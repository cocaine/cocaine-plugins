/*
    Copyright (c) 2016+ Anton Matveenko <antmat@me.com>
    Copyright (c) 2016+ Other contributors as noted in the AUTHORS file.

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

#include <cocaine/dynamic.hpp>
#include <cocaine/idl/primitive.hpp>

#include <map>
#include <string>

namespace cocaine {
namespace io {

struct metrics_tag;

struct metrics {

    struct fetch {
        typedef metrics_tag tag;

        constexpr static auto alias() noexcept -> const char* {
            return "fetch";
        }

        typedef boost::mpl::vector<
         /* Output type. Allowed plain (default) and json tree. */
            optional<std::string>,
         /* Query AST. */
            optional<dynamic_t>
        >::type argument_type;

        typedef option_of<
            dynamic_t
        >::tag upstream_type;
    };

};

template<>
struct protocol<metrics_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        metrics::fetch
    >::type messages;
};

}  // namespace io
}  // namespace cocaine
