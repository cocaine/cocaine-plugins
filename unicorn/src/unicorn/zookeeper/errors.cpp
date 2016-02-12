/*
    Copyright (c) 2011-2015 Anton Matveenko <antmat@yandex-team.ru>
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

#include "cocaine/detail/zookeeper/errors.hpp"

#include <string>

namespace cocaine { namespace error {

class zookeeper_category_t:
    public std::error_category
{
    virtual
    auto
    name() const throw() -> const char* {
        return "cocaine.plugins.zookeeper";
    }

    virtual
    auto
    message(int code) const -> std::string {
        return std::string("zookeeper: ") + zerror(code);
    }
};

auto
zookeeper_category() -> const std::error_category& {
    static zookeeper_category_t instance;
    return instance;
}

auto
make_error_code(zookeeper_errors code) -> std::error_code {
    return std::error_code(static_cast<int>(code), zookeeper_category());
}

}}
