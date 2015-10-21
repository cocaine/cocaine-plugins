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

namespace cocaine { namespace io {

template<>
struct type_traits<logging::filter_t> {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const logging::filter_t& source) {
        type_traits<dynamic_t>::pack(target, source.representation());
    }

    static inline
    void
    unpack(const msgpack::object& source, logging::filter_t& target) {
        dynamic_t representation;
        type_traits<dynamic_t>::unpack(source, representation);
        target = logging::filter_t(representation);
    }
};


}} // namespace cocaine::io

