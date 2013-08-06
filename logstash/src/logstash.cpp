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

#include "cocaine/asio/resolver.hpp"

#include "cocaine/context.hpp"

#include <ctime>
#include <system_error>

#include <sys/time.h>

using namespace cocaine::logging;

logstash_t::logstash_t(const config_t& config, const Json::Value& args):
    category_type(config, args),
    m_config(config)
{
    if(args["port"].empty()) {
        throw cocaine::error_t("no logstash port has been specified");
    }

    const auto host = args.get("host", "127.0.0.1").asString();
    const auto port = args["port"].asUInt();

    std::vector<io::udp::endpoint> endpoints;

    try {
        endpoints = io::resolver<io::udp>::query(host, port);
    } catch(const std::system_error& e) {
        throw cocaine::error_t(
            "unable to resolve any logstash server endpoints - [%d] %s",
            e.code().value(),
            e.code().message()
        );
    }

    for(auto it = endpoints.begin(); it != endpoints.end(); ++it) {
        try {
            m_socket.reset(new io::socket<io::udp>(*it));
        } catch(const std::system_error& e) {
            continue;
        }

        break;
    }

    if(!m_socket) {
        throw cocaine::error_t("unable to connect to '%s:%d'", host, port);
    }
}

namespace {

const char* describe[] = {
    nullptr,
    "ERROR",
    "WARNING",
    "INFO",
    "DEBUG"
};

}

std::string
logstash_t::prepare_output(logging::priorities level, const std::string& source, const std::string& message) {
    struct timeval time;
    struct tm      timeinfo;

    std::memset(&time,     0, sizeof(time));
    std::memset(&timeinfo, 0, sizeof(timeinfo));

    ::gettimeofday(&time, NULL);
    ::gmtime_r(&time.tv_sec, &timeinfo);

    char timestamp[128] = { 0 };

    if(std::strftime(timestamp, 128, "%FT%T", &timeinfo) == 0) {
        // Do nothing.
    }

    Json::Value root(Json::objectValue);
    Json::Value tags(Json::arrayValue);
    Json::Value fields(Json::objectValue);

    fields["level"] = describe[level];
    fields["uuid"] = m_config.network.uuid;

    root["@fields"] = fields;
    root["@message"] = message;
    root["@source"] = cocaine::format("udp://%s", m_socket->local_endpoint());
    root["@source_host"] = m_config.network.hostname;
    root["@source_path"] = source;
    root["@tags"] = tags;
    root["@timestamp"] = cocaine::format("%s.%06ldZ", timestamp, time.tv_usec);

    Json::FastWriter writer;

    return writer.write(root);
}

void
logstash_t::emit(logging::priorities level, const std::string& source, const std::string& message) {
    const std::string& json = prepare_output(level, source, message);
    std::error_code code;

    m_socket->write(json.c_str(), json.size(), code);

    if(code) {
        //!@note: Do something.
    }
}
