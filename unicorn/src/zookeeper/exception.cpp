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

#include "cocaine/zookeeper/exception.hpp"
#include "cocaine/zookeeper/zookeeper.hpp"
namespace zookeeper {
exception::exception(std::string _message, int _zk_error_code) :
    message(std::move(_message)),
    zk_error_code(_zk_error_code)
{
    message += "Zookeeper error(" + std::to_string(zk_error_code) + "): " + get_error_message(zk_error_code);
}

exception::exception(int _zk_error_code) :
    message(),
    zk_error_code(_zk_error_code)
{
    message += "Zookeeper error(" + std::to_string(zk_error_code) + "): " + get_error_message(zk_error_code);
}

int exception::code() const {
    return zk_error_code;
}

const char* exception::what() const noexcept {
    return message.c_str();
}
}