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

#include "cocaine/traits/dynamic.hpp"
#include "cocaine/unicorn/value.hpp"

#include <cocaine/traits.hpp>

namespace cocaine { namespace io {

template<>
struct type_traits<cocaine::unicorn::versioned_value_t> {
template<class Stream>
static inline
void
pack(msgpack::packer<Stream>& packer, const cocaine::unicorn::versioned_value_t& source) {
    packer.pack_array(2);
    cocaine::io::type_traits<cocaine::unicorn::value_t>::pack(packer, source.value());
    cocaine::io::type_traits<cocaine::unicorn::version_t>::pack(packer, source.version());
}

static inline
void
unpack(const msgpack::object& source, cocaine::unicorn::versioned_value_t& target) {
    if(source.type != msgpack::type::ARRAY) {
        throw msgpack::type_error();
    }
    cocaine::unicorn::value_t value;
    cocaine::unicorn::version_t version;
    type_traits<cocaine::unicorn::value_t>::unpack(source.via.array.ptr[0], value);
    type_traits<cocaine::unicorn::version_t>::unpack(source.via.array.ptr[1], version);
    target = cocaine::unicorn::versioned_value_t(std::move(value), version);
}
};

}} // namespace cocaine::io
