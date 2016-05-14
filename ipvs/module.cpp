/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#include "gateway.hpp"

#include <cocaine/repository.hpp>
#include <cocaine/repository/gateway.hpp>

using namespace cocaine;
using namespace cocaine::gateway;

extern "C" {
    auto
    validation() -> api::preconditions_t {
        return api::preconditions_t { COCAINE_MAKE_VERSION(0, 12, 0) };
    }

    void
    initialize(api::repository_t& repository) {
        repository.insert<ipvs_t>("ipvs");
    }
}
