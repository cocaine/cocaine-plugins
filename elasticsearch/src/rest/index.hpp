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

#pragma once

#include <rapidjson/document.h>

#include <cocaine/logging.hpp>

#include "cocaine/service/elasticsearch.hpp"

namespace cocaine { namespace service {

struct index_handler_t {
    std::shared_ptr<cocaine::logging::log_t> log;

    template<typename Deferred = cocaine::deferred<response::index>>
    void
    operator()(Deferred &deferred, int code, const std::string &data) const {
        if (log)
            COCAINE_LOG_DEBUG(log, "Index request completed [%d]", code);

        rapidjson::Document root;
        root.Parse<0>(data.c_str());
        if (root.HasParseError())
            return deferred.abort(-1, cocaine::format("parsing failed - %s", root.GetParseError()));

        if (!root.HasMember("_id"))
            return deferred.write(std::make_tuple(false, std::string("error - response has no field '_id'")));

        if (log)
            COCAINE_LOG_DEBUG(log, "Received data: %s", data);

        deferred.write(std::make_tuple(true, std::string(root["_id"].GetString())));
    }
};

} }
