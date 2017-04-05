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

#include <zookeeper/zookeeper.h>

namespace zookeeper {

/**
* Class representing zookeeper clientid_t from C api.
*/
class session_t {
public:
    session_t();

    const clientid_t*
    native();

    /**
    * Reset session. Used to reset expired sesions
    */
    void
    reset();

    /**
    * Assign native handle to session
    */
    void
    assign(const clientid_t& native);

    /**
    * Check if session was filled with valid native handle
    */
    bool
    valid() const;
    
private:
    clientid_t zk_session;
};

} //namespace zookeeper
