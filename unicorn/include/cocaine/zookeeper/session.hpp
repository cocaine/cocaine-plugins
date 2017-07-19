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

namespace cocaine {
namespace zookeeper {

/**
* Class representing zookeeper clientid_t from C api.
*/
class session_t {
public:
    session_t();

    auto native() -> const clientid_t*;

    /**
    * Reset session. Used to reset expired sesions
    */
    auto reset() -> void;

    /**
    * Assign native handle to session
    */
    auto assign(const clientid_t& native) -> void;

    /**
    * Check if session was filled with valid native handle
    */
    auto valid() const -> bool;
    
private:
    clientid_t zk_session;
};

} //namespace zookeeper
} //namespace cocaine
