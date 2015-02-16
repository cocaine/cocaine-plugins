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
#ifndef ZOOKEEPER_SESSION_HPP
#define ZOOKEEPER_SESSION_HPP
#include <zookeeper/zookeeper.h>
namespace zookeeper {
class session_t {
public:
    session_t();
    const clientid_t* native();
    void reset();
    void assign(const clientid_t* native);
private:
    clientid_t zk_session;
    bool valid() const;
};
}
#endif