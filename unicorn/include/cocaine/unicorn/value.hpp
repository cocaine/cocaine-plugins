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

#ifndef COCAINE_UNICORN_VALUE_HPP
#define COCAINE_UNICORN_VALUE_HPP

#include "cocaine/zookeeper/zookeeper.hpp"

#include <cocaine/common.hpp>
#include <cocaine/dynamic.hpp>
#include <cocaine/traits/dynamic.hpp>

#include <msgpack.hpp>

#include <string>

namespace cocaine { namespace unicorn {

typedef zookeeper::version_t version_t;
typedef cocaine::dynamic_t value_t;

/**
* Serializes service representation of value to zookepeers representation.
* Currently ZK store msgpacked data, and service uses cocaine::dynamic_t
*/
zookeeper::value_t
serialize(const value_t& val);

/**
* Unserializes zookepeers representation to service representation.
*/
value_t
unserialize(const zookeeper::value_t& val);

class versioned_value_t {
public:
    versioned_value_t() = default;
    versioned_value_t(const versioned_value_t& other) = default;
    versioned_value_t(value_t _value, version_t _version);

    template<class Stream>
    void
    msgpack_pack(msgpack::packer<Stream>& packer) const {
        packer.pack_array(2);
        cocaine::io::type_traits<value_t>::pack(packer, value);
        cocaine::io::type_traits<version_t>::pack(packer, version);
    }
private:
    value_t value;
    version_t version;
};

}}
#endif