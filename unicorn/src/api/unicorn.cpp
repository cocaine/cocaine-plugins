/*
* 2015+ Copyright (c) Anton Matveenko <antmat@me.com>
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

#include "unicorn.hpp"

#include <cocaine/context.hpp>

namespace cocaine { namespace api {

/**
 * Unicorn trait for service creation.
 * Trait to create unicorn service by name. All instances are cached by name as it is done in storage.
 */
category_traits<unicorn_t>::ptr_type
unicorn(context_t& context, const std::string& name) {
    auto it = context.config.services.find(name);

    if(it == context.config.services.end()) {
        throw std::system_error(std::make_error_code(std::errc::argument_out_of_domain), name);
    }

    return context.get<unicorn_t>(it->second.type, context, name, it->second.args);
}

}} // namespace cocaine::api
