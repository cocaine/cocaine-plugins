#include <cocaine/logging.hpp>

#include "delete.hpp"

#define UNUSED(o) \
    (void)o;

const uint16_t HTTP_OK = 200;
const uint16_t HTTP_ACCEPTED = 202;

using namespace cocaine::service;

void
delete_handler_t::operator ()(cocaine::deferred<response::delete_index> deferred, int code, const std::string &data) const
{
    UNUSED(data);
    COCAINE_LOG_DEBUG(log, "Delete request completed [%d]", code);

    deferred.write(code == HTTP_OK || code == HTTP_ACCEPTED);
}
