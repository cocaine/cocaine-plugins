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

#include <cocaine/common.hpp>
#include <cocaine/dynamic.hpp>
#include <cocaine/traits/dynamic.hpp>

#include <msgpack.hpp>

namespace cocaine { namespace unicorn {

typedef long long version_t;
typedef cocaine::dynamic_t value_t;

static constexpr version_t MIN_VERSION = -2;
static constexpr version_t NOT_EXISTING_VERSION = -1;



class versioned_value_t {
public:
    versioned_value_t() = default;
    versioned_value_t(const versioned_value_t&) = default;
    versioned_value_t(value_t _value, version_t _version);

    const value_t&
    get_value() const {
        return value;
    }

    version_t
    get_version() const {
        return version;
    }

private:
    value_t value;
    version_t version;
};

}}
