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
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include <blackhole/logger.hpp>

#include <cocaine/logging.hpp>
#include <cocaine/traits/tuple.hpp>

#include "cocaine/service/elasticsearch/global.hpp"
#include "cocaine/service/elasticsearch.hpp"

namespace cocaine { namespace service {

struct search_handler_t {
    std::shared_ptr<logging::logger_t> log;

    template<typename Deferred = cocaine::deferred<response::search>>
    void
    operator()(Deferred& deferred, int code, const std::string& data) const {
        if (log) {
            COCAINE_LOG_DEBUG(log, "Search request completed [{}]", code);
        }

        rapidjson::Document root;
        root.Parse<0>(data.c_str());
        if (root.HasParseError()) {
            deferred.abort(asio::error::operation_aborted, cocaine::format("parsing failed - {}", root.GetParseError()));
            return;
        }

        if (code == HTTP_OK) {
            if (root.HasMember("hits")) {
                const rapidjson::Value &hits = root["hits"];
                rapidjson::GenericStringBuffer<rapidjson::UTF8<>> buffer;
                rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF8<>>> writer(buffer);
                hits["hits"].Accept(writer);
                const int total = hits["total"].GetInt();
                deferred.write(std::make_tuple(true, total, std::string(buffer.GetString())));
            } else {
                deferred.write(std::make_tuple(false, 0, std::string()));
            }
        } else {
            std::string reason = cocaine::format("{}[{}]", root["error"].GetString(), code);
            deferred.write(std::make_tuple(false, 0, reason));
        }
    }
};

} }
