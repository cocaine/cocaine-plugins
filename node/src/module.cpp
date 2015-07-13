#include "cocaine/service/node.hpp"

using namespace cocaine;
using namespace cocaine::service;

extern "C" {
    auto
    validation() -> api::preconditions_t {
        return api::preconditions_t { COCAINE_MAKE_VERSION(0, 12, 2) };
    }

    void
    initialize(api::repository_t& repository) {
        repository.insert<node_t>("node::v2");
    }
}
