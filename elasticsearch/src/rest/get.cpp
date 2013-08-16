/*
    Copyright (c) 2011-2013 Evgeny Safronov <esafronov@yandex-team.ru>
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

#include <cocaine/traits/tuple.hpp>
#include <cocaine/logging.hpp>

#include "handlers.hpp"
#include "get.hpp"

using namespace cocaine::service;

void
get_handler_t::operator ()(cocaine::deferred<response::get> deferred, int code, const std::string &data) const {
    if (code == 200) {
        deferred.write(std::make_tuple(true, data));
    } else {
        Json::Value root;
        Json::Reader reader;
        bool parsingSuccessful = reader.parse(data, root);
        if (!parsingSuccessful)
            return deferred.abort(-1, "parsing failed");

        std::string reason = cocaine::format("%s[%d]", root["error"].asString(), code);
        deferred.write(std::make_tuple(false, reason));
    }
}
