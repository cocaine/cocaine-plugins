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

#include <string>

namespace cocaine { namespace unicorn {
typedef unsigned long version_t ;
typedef std::function<void(const versioned_value_t&)> subscribe_callback_t;
class value_base {
public:
    std::string serialize() const;
protected:
    ~value_holder();
};

class value_t : public value_base {

};

class numeric_value_t : public value_base {

};

class versioned_value_t : public value_base {

};
}}
#endif