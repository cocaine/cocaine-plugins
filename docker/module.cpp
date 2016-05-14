/*
    Copyright (c) 2011-2013 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#include "docker_client.hpp"
#include "isolate.hpp"

#include <cocaine/repository.hpp>
#include <cocaine/repository/isolate.hpp>

using namespace cocaine;
using namespace cocaine::isolate;

namespace cocaine { namespace error {

const std::error_category&
docker_curl_category();

constexpr size_t docker_curl_category_id = 0x50ff;
}} // namespace cocaine::error

extern "C" {
    auto
    validation() -> api::preconditions_t {
        return api::preconditions_t { COCAINE_MAKE_VERSION(0, 12, 0) };
    }

    void
    initialize(api::repository_t& repository) {
        repository.insert<docker_t>("legacy_docker");
        error::registrar::add(error::docker_curl_category(), error::docker_curl_category_id);
    }
}
