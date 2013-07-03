#include "cocaine/loggers/logstash.hpp"

using namespace cocaine;
using namespace cocaine::logging;

extern "C" {
    void
    initialize(api::repository_t& repository) {
        repository.insert<cocaine::logging::logstash_t>("logstash");
    }
}
