#include "cocaine/loggers/logstash.hpp"

#include <system_error>

using namespace cocaine::logging;

logstash_t::logstash_t(const Json::Value &args) :
    api::logger_t(args)
{
    std::string host = args.get("host", "0.0.0.0").asString();
    int16_t port = args["port"].asInt();
    socket = io::socket<io::udp>(io::udp::endpoint(host, port)); //! can throw
}

std::string logstash_t::prepare_output(logging::priorities level, const std::string& source, const std::string& message) const
{
    Json::Value root;
    root["level"] = level_description[level];
    root["source"] = source;
    root["message"] = message;
    Json::FastWriter writer;
    return writer.write(root);
}

void
logstash_t::emit(logging::priorities level, const std::string& source, const std::string& message) {
    const std::string &json = prepare_output(level, source, message);
    std::error_code code;
    socket.write(json.c_str(), json.size(), code);

    if (code.value() != 0) {
        //!@note: Do something
    }
}



