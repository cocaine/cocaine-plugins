#include <cocaine/traits/tuple.hpp>
#include <cocaine/logging.hpp>

#include "handlers.hpp"
#include "get.hpp"

using namespace cocaine::service;

void get_handler_t::operator ()(cocaine::deferred<response::get> deferred, int code, const std::string &data) const
{
    if (code == 200) {
        deferred.write(std::make_tuple(true, data));
    } else {
        Json::Value root;
        Json::Reader reader;
        bool parsingSuccessful = reader.parse(data, root);
        if (!parsingSuccessful)
            return deferred.abort(-1, "parsing failed");

        std::string reason = cocaine::format("%s[%d]", root["error"].asString(), code);
        deferred.write(std::make_tuple(false, reason));
    }
}
