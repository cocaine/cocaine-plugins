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

#include "cocaine/loggers/logstash.hpp"

#include <system_error>

using namespace cocaine::logging;

logstash_t::logstash_t(const Json::Value& args):
    api::logger_t(args)
{
    std::string host = args.get("host", "0.0.0.0").asString();
    int16_t port = args["port"].asInt();
    socket = io::socket<io::udp>(io::udp::endpoint(host, port)); //! can throw
}

std::string logstash_t::prepare_output(logging::priorities level, const std::string& source, const std::string& message) const {
    Json::Value root;
    root["level"] = level_description[level];
    root["source"] = source;
    root["message"] = message;
    Json::FastWriter writer;
    return writer.write(root);
}

void
logstash_t::emit(logging::priorities level, const std::string& source, const std::string& message) {
    const std::string& json = prepare_output(level, source, message);
    std::error_code code;
    socket.write(json.c_str(), json.size(), code);

    if (code.value() != 0) {
        //!@note: Do something
    }
}

