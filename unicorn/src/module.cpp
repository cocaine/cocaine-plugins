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

#include "cocaine/detail/unicorn/zookeeper.hpp"

#include "cocaine/detail/zookeeper/errors.hpp"

#include "cocaine/service/unicorn.hpp"

#include <cocaine/errors.hpp>
#include <cocaine/repository.hpp>
#include <cocaine/repository/cluster.hpp>
#include <cocaine/repository/service.hpp>
#include <cocaine/repository/unicorn.hpp>

using namespace cocaine;
extern "C" {
auto
validation() -> api::preconditions_t {
    return api::preconditions_t {COCAINE_MAKE_VERSION(0, 12, 0)};
}

void
initialize(api::repository_t& repository) {
    repository.insert<service::unicorn_service_t>("unicorn");
    repository.insert<cluster::unicorn_cluster_t>("unicorn");
    repository.insert<unicorn::zookeeper_t>("zookeeper");
    error::registrar::add(error::zookeeper_category(), error::zookeeper_category_id);
}

}
