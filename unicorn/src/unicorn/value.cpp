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

#include "cocaine/unicorn/value.hpp"
#include "cocaine/zookeeper/exception.hpp"

namespace cocaine { namespace unicorn {

versioned_value_t::versioned_value_t(value_t _value, version_t _version) :
    value(std::move(_value)),
    version(std::move(_version))
{}

zookeeper::value_t serialize(const value_t& val) {
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> packer(buffer);
    cocaine::io::type_traits<cocaine::dynamic_t>::pack(packer, val);
    return std::string(buffer.data(), buffer.size());
}

value_t unserialize(const zookeeper::value_t& val) {
    msgpack::unpacked unpacked;

    try {
        unpacked = msgpack::unpack(val.c_str(), val.size());
    } catch(const msgpack::unpack_error& e) {
        throw zookeeper::exception(zookeeper::ZOO_EXTRA_ERROR::INVALID_VALUE);
    }

    value_t target;
    cocaine::io::type_traits<cocaine::dynamic_t>::unpack(unpacked.get(), target);
    return target;
}
}}
