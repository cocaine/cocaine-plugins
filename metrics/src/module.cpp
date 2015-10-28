#include <cocaine/repository.hpp>

#include "cocaine/service/metrics.hpp"

extern "C" {
    auto
    validation() -> cocaine::api::preconditions_t {
        return cocaine::api::preconditions_t { COCAINE_MAKE_VERSION(0, 12, 3) };
    }

    void
    initialize(cocaine::api::repository_t& repository) {
        repository.insert<cocaine::service::metrics_t>("metrics");
    }
}
