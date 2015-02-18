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

#include <zookeeper/zookeeper.h>
#include <algorithm>
#include "cocaine/zookeeper/session.hpp"
namespace zookeeper {

session_t::session_t() :
    zk_session()
{
}

const clientid_t* session_t::native() {
    if(valid()) {
        return &zk_session;
    }
    return nullptr;
}

void session_t::reset() {
    zk_session.client_id = 0;
    std::fill(zk_session.passwd, zk_session.passwd+16, 0);
}

bool session_t::valid() const {
    return zk_session.client_id != 0;
}

void session_t::assign(const clientid_t* native) {
    zk_session.client_id = native->client_id;
    std::copy(native->passwd, native->passwd+16, zk_session.passwd);
}
}