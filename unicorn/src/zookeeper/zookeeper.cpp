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

#include "cocaine/zookeeper/zookeeper.hpp"

#include <stdexcept>

namespace zookeeper {
path_t path_parent(const path_t& path, unsigned int depth) {
    size_t last_char = path.size();
    size_t pos = 0;
    for (size_t i = 0; i <= depth; i++) {
        pos = path.find_last_of('/', last_char);
        if (pos == path.size() - 1) {
            last_char = pos - 1;
            pos = path.find_last_of('/', last_char);
        }
        if (pos == std::string::npos || pos == 0) {
            throw std::runtime_error("could not get " + std::to_string(depth) + "th parent from path: " + path);
        }
        last_char = pos - 1;
    }
    return path.substr(0, pos);
}
}
