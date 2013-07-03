#pragma once

#include <cocaine/asio/udp.hpp>
#include <cocaine/asio/socket.hpp>
#include <cocaine/api/logger.hpp>

namespace cocaine { namespace logging {

const std::string level_description[] = {
    "IGNORE",
    "ERROR",
    "WARNING",
    "INFO",
    "DEBUG"
};

class logstash_t : public api::logger_t {
    io::socket<io::udp> socket;
public:
    logstash_t(const Json::Value& args);

    void
    emit(logging::priorities level, const std::string& source, const std::string& message);

private:
    std::string prepare_output(logging::priorities level, const std::string& source, const std::string& message) const;
};

}
}
