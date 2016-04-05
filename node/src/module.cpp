#include <cocaine/detail/isolate/external.hpp>
#include <cocaine/repository.hpp>
#include <cocaine/repository/service.hpp>

#include "cocaine/repository/isolate.hpp"
#include "cocaine/service/node.hpp"

#include "cocaine/detail/isolate/process.hpp"

using namespace cocaine;
using namespace cocaine::service;

extern "C" {

auto validation() -> api::preconditions_t {
    return api::preconditions_t{COCAINE_MAKE_VERSION(0, 12, 5)};
}

void initialize(api::repository_t& repository) {
    repository.insert<isolate::process_t>("legacy_process");
    repository.insert<node_t>("node::v2");
    repository.insert<isolate::external_t>("external");
    repository.insert<isolate::external_t>("process");
    repository.insert<isolate::external_t>("docker");
    repository.insert<isolate::external_t>("porto");
}

}  // extern "C"
