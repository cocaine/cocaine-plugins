/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/service/node/manifest.hpp"

#include <cocaine/context/config.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/format.hpp>
#include <cocaine/traits/dynamic.hpp>

#include <unistd.h>

using namespace cocaine;

manifest_t::manifest_t(context_t& context, const std::string& name_):
    cached<dynamic_t>(context, "manifests", name_),
    name(name_)
{
    endpoint = cocaine::format("{}/{}.{}", context.config().path().runtime(), name, ::getpid());

    environment = as_object().at("environment", dynamic_t::object_t()).to<std::map<std::string, std::string>>();

    if(as_object().find("slave") != as_object().end()) {
        executable = as_object().at("slave").as_string();
    } else {
        throw cocaine::error_t("runnable object has not been specified");
    }
}
