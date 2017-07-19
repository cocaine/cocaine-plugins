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

#include "cocaine/cluster/unicorn.hpp"

#include "cocaine/unicorn/zookeeper.hpp"

#include "cocaine/service/unicorn.hpp"

#include <cocaine/errors.hpp>
#include <cocaine/repository.hpp>
#include <cocaine/repository/authorization.hpp>
#include <cocaine/repository/cluster.hpp>
#include <cocaine/repository/service.hpp>
#include <cocaine/repository/unicorn.hpp>

#include "authorization/unicorn.hpp"
#include "repository/zookeeper.hpp"

using namespace cocaine;

extern "C" {

auto validation() -> api::preconditions_t {
    return api::preconditions_t{ COCAINE_VERSION };
}


auto initialize(api::repository_t& repository) -> void {
    repository.insert<unicorn::zookeeper_t>("zookeeper", std::make_unique<api::zookeeper_factory_t>());
    repository.insert<cluster::unicorn_cluster_t>("unicorn");
    repository.insert<authorization::unicorn::enabled_t>("unicorn");
    repository.insert<service::unicorn_service_t>("unicorn");
}

}
