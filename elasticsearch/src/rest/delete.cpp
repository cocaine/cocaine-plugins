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

#include <cocaine/logging.hpp>

#include "delete.hpp"

#define UNUSED(o) \
    (void)o;

const uint16_t HTTP_OK = 200;
const uint16_t HTTP_ACCEPTED = 202;

using namespace cocaine::service;

void
delete_handler_t::operator ()(cocaine::deferred<response::delete_index> deferred, int code, const std::string &data) const {
    UNUSED(data);
    COCAINE_LOG_DEBUG(log, "Delete request completed [%d]", code);

    deferred.write(code == HTTP_OK || code == HTTP_ACCEPTED);
}
