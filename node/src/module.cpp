#include <cocaine/repository.hpp>
#include <cocaine/repository/service.hpp>

#include "cocaine/repository/auth.hpp"
#include "cocaine/repository/isolate.hpp"
#include "cocaine/service/node.hpp"
#include "cocaine/service/node/error.hpp"
#include "cocaine/service/node/slave/error.hpp"

#include "cocaine/detail/isolate/external.hpp"
#include "cocaine/detail/isolate/process.hpp"

#include "auth/anonymous.hpp"

using namespace cocaine;
using namespace cocaine::service;

extern "C" {

auto validation() -> cocaine::api::preconditions_t {
    return cocaine::api::preconditions_t{COCAINE_VERSION};
}

void initialize(cocaine::api::repository_t& repository) {
    repository.insert<isolate::process_t>("legacy_process");

    repository.insert<node_t>("node::v2");
    repository.insert<isolate::external_t>("external");
    repository.insert<isolate::external_t>("process");
    repository.insert<isolate::external_t>("docker");
    repository.insert<isolate::external_t>("porto");

    repository.insert<auth::anonymous_t>("anonymous");

    error::registrar::add(error::node_category(), error::node_category_id);
    error::registrar::add(error::slave_category(), error::slave_category_id);
    error::registrar::add(error::overseer_category(), error::overseer_category_id);

    error::registrar::add(node::worker_category(), node::worker_category_t::id());
    error::registrar::add(node::worker_user_category(), node::worker_user_category_t::id());
}

}  // extern "C"
