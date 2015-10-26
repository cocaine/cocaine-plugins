#include "cocaine/repository.hpp"

extern "C" {
    auto
    validation() -> cocaine::api::preconditions_t {
        return cocaine::api::preconditions_t { COCAINE_MAKE_VERSION(0, 12, 4) };
    }

    void
    initialize(cocaine::api::repository_t& repository) {
    }
}
