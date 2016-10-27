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

#include "cocaine/api/sender.hpp"
#include "cocaine/repository/sender.hpp"
#include "cocaine/sender/null.hpp"
#include "cocaine/sender/postgres.hpp"
#include "cocaine/service/metrics.hpp"

#include <cocaine/api/service.hpp>
#include <cocaine/repository.hpp>
#include <cocaine/repository/service.hpp>

using namespace cocaine;
extern "C" {

auto validation() -> api::preconditions_t {
    return api::preconditions_t{ COCAINE_VERSION };
}

auto initialize(api::repository_t& repository) -> void {
    repository.insert<service::metrics_t>("metrics");
    repository.insert<sender::pg_sender_t>("postgres");
    repository.insert<sender::null_sender_t>("null");
}

}
