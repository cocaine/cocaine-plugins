/*
* 2016+ Copyright (c) Anton Matveenko <antmat@yandex-team.ru>
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

#include <cocaine/api/postgres/pool.hpp>
#include <cocaine/api/storage.hpp>
#include <cocaine/repository.hpp>
#include <cocaine/repository/postgres/pool.hpp>
#include <cocaine/repository/storage.hpp>

#include "cocaine/storage/postgres.hpp"

using namespace cocaine;
extern "C" {

api::preconditions_t
validation() {
    return api::preconditions_t{ COCAINE_VERSION };
}

void
initialize(api::repository_t& repository) {
    repository.insert<storage::postgres_t>("postgres");
    repository.insert<postgres::pool_t>("postgres");
}

}
