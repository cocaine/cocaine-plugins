#include <cocaine/traits/tuple.hpp>
#include <cocaine/logging.hpp>

#include "handlers.hpp"
#include "index.hpp"

using namespace cocaine::service;

void index_handler_t::operator ()(cocaine::deferred<response::index> deferred, int code, const std::string &data) const
{
    COCAINE_LOG_DEBUG(log, "Index request completed [%d]", code);

    Json::Value root;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(data, root);
    if (!parsingSuccessful) {
        return deferred.abort(-1, "parsing failed");
    }

    std::string id = root["_id"].asString();
    COCAINE_LOG_DEBUG(log, "Received data: %s", data);

    deferred.write(std::make_tuple(true, id));
}
