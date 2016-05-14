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

#include "storage.hpp"

#include <cocaine/repository.hpp>
#include <cocaine/repository/storage.hpp>

#include <mongo/client/init.h>

using namespace cocaine;
using namespace cocaine::storage;

extern "C" {
    void
    initialize(api::repository_t& repository) {
        const auto status = mongo::client::initialize();

        if(status != mongo::Status::OK()) {
            throw cocaine::error_t("unable to initialize mongodb - {}", status.toString());
        }

        repository.insert<mongo_storage_t>("mongodb");
    }
}

