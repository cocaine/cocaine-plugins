#include <cocaine/traits/tuple.hpp>
#include <cocaine/logging.hpp>

#include "handlers.hpp"
#include "search.hpp"

using namespace cocaine::service;

void search_handler_t::operator ()(cocaine::deferred<response::search> deferred, int code, const std::string &data) const
{
    COCAINE_LOG_DEBUG(log, "Search request completed [%d]", code);

    Json::Value root;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(data, root);
    if (!parsingSuccessful)
        return deferred.abort(-1, "parsing failed");

    if (code == 200) {
        Json::FastWriter writer;
        Json::Value hits = root["hits"];
        const int total = hits["total"].asInt();
        deferred.write(std::make_tuple(true, total, writer.write(hits["hits"])));
    } else {
        std::string reason = cocaine::format("%s[%d]", root["error"].asString(), code);
        deferred.write(std::make_tuple(false, 0, reason));
    }
}
