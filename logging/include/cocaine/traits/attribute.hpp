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

#include "cocaine/logging/attribute.hpp"

#include <cocaine/common.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/traits.hpp>

#include <cocaine/traits/dynamic.hpp>

namespace cocaine {
namespace io {

template <>
struct type_traits<logging::attribute_t> {
    template <class Stream>
    static inline void pack(msgpack::packer<Stream>& target, const logging::attribute_t& source) {
        target.pack_array(2);
        type_traits<logging::attribute_t::name_t>::pack(target, source.name);
        type_traits<logging::attribute_t::value_t>::pack(target, source.value);
    }

    static inline void unpack(const msgpack::object& source, logging::attribute_t& target) {
        if (source.type == msgpack::type::ARRAY && source.via.array.size == 2) {
            type_traits<logging::attribute_t::name_t>::unpack(source.via.array.ptr[0], target.name);
            type_traits<logging::attribute_t::value_t>::unpack(source.via.array.ptr[1],
                                                               target.value);
        } else if (source.type == msgpack::type::MAP && source.via.map.size == 1) {
            type_traits<logging::attribute_t::name_t>::unpack(source.via.map.ptr->key, target.name);
            type_traits<logging::attribute_t::value_t>::unpack(source.via.map.ptr->val,
                                                               target.value);
        } else {
            throw cocaine::error::error_t("invalid source for attribute pair : %s", source.type);
        }
    }
};
}
}  // namespace cocaine::io
