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

#include <cocaine/repository.hpp>
#include <cocaine/repository/service.hpp>

#include "cocaine/service/logging.hpp"

using namespace cocaine;
using namespace cocaine::service;

extern "C" {
auto validation() -> api::preconditions_t {
    return api::preconditions_t{ COCAINE_VERSION };
}

void initialize(api::repository_t& repository) {
    repository.insert<logging_v2_t>("logging::v2");
}
}
