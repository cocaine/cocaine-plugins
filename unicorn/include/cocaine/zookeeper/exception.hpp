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
#ifndef ZOOKEEPER_EXCEPTION_HPP
#define ZOOKEEPER_EXCEPTION_HPP

#include <exception>
#include <string>

namespace zookeeper {
class exception :
    public std::exception
{
public:
    // You can specify some prefix.
    // what() will return "$your_prefixZookeeper error($zk_error_code) : $description_of_zk_error_code"
    exception(std::string message_prefix, int zk_error_code);
    exception(int zk_error_code);
    int code() const;
    virtual const char* what() const noexcept;
    virtual ~exception() noexcept {}
private:
    int zk_error_code;
    std::string message;
};
}
#endif
