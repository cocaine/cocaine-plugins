#pragma once

#include "cocaine/service/elasticsearch.hpp"

namespace cocaine { namespace service {

struct index_handler_t {
    std::shared_ptr<cocaine::logging::log_t> log;

    void operator()(cocaine::deferred<response::index> deferred, int code, const std::string &data) const;
};

} }
