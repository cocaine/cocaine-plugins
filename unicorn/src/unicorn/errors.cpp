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

#include "cocaine/unicorn/errors.hpp"

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
        return zerror(code);
    }
};

class unicorn_category_t:
    public std::error_category
{
    virtual
    auto
    name() const throw() -> const char* {
        return "cocaine.plugins.unicorn";
    }

    virtual
    auto
    message(int code) const -> std::string {
        switch (code) {
            case CHILD_NOT_ALLOWED :
                return "Can not get value of a node with childs";
            case INVALID_TYPE :
                return "Invalid type of value stored for requested operation";
            case INVALID_VALUE :
                return "Could not unserialize value stored in zookeeper";
            case COULD_NOT_CONNECT:
                return "Could not reconnect to zookeper. Current errno:" + std::to_string(errno);
            case UNKNOWN_ERROR:
                return "Unknown zookeeper error";
            case HANDLER_SCOPE_RELEASED:
                return "Handler scope was released";
            case INVALID_NODE_NAME:
                return "Inavlid node name specified";
            case INVALID_PATH:
                return "Inavlid path specified";
            case VERSION_NOT_ALLOWED:
                return "Specified version is not allowed for command";
            case INVALID_CONNECTION_ENDPOINT:
                return "Invalid connection endpoint specified";
            default:
                return "Unknow unicorn error";
        }
    }
};

auto
zookeeper_category() -> const std::error_category& {
    static zookeeper_category_t instance;
    return instance;
}

auto
unicorn_category() -> const std::error_category& {
    static unicorn_category_t instance;
    return instance;
}

auto
make_error_code(zookeeper_errors code) -> std::error_code {
    return std::error_code(static_cast<int>(code), zookeeper_category());
}

auto
make_error_code(unicorn_errors code) -> std::error_code {
    return std::error_code(static_cast<int>(code), unicorn_category());
}
}}
