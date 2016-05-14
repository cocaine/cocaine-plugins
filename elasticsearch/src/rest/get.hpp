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

#include "cocaine/service/elasticsearch/global.hpp"
#include "cocaine/service/elasticsearch/config.hpp"
#include "cocaine/service/elasticsearch.hpp"

namespace cocaine { namespace service {

struct get_handler_t {
    std::shared_ptr<logging::logger_t> log;

    template<typename Deferred = cocaine::deferred<response::get>>
    void
    operator()(Deferred& deferred, int code, const std::string& data) const {
        if (code == HTTP_OK) {
            deferred.write(std::make_tuple(true, data));
        } else {
            rapidjson::Document root;
            root.Parse<0>(data.c_str());
            if (root.HasParseError()) {
                deferred.abort(asio::error::operation_aborted, cocaine::format("parsing failed - {}", root.GetParseError()));
                return;
            }

            const std::string& error = root.HasMember("error") ? root["error"].GetString() : "";
            const std::string& reason = cocaine::format("{}[{}]", error, code);
            deferred.write(std::make_tuple(false, reason));
        }
    }
};

} }
