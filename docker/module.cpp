#include "isolate.hpp"

using namespace cocaine;
using namespace cocaine::isolate;

extern "C" {
    void
    initialize(api::repository_t& repository) {
        repository.insert<docker_t>("docker");
    }
}
