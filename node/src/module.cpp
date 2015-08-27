#include "cocaine/service/node.hpp"

#include "cocaine/detail/isolate/process.hpp"

#ifdef COCAINE_ALLOW_CGROUPS
    #include "cocaine/errors.hpp"

    #include <boost/lexical_cast.hpp>
    #include <libcgroup.h>
    #include <atomic>
#endif


using namespace cocaine;
using namespace cocaine::service;

extern "C" {
    auto
    validation() -> api::preconditions_t {
        return api::preconditions_t { COCAINE_MAKE_VERSION(0, 12, 2) };
    }

    void
    initialize(api::repository_t& repository) {
#ifdef COCAINE_ALLOW_CGROUPS
        // Initialize cgroups only once during module load.
        int rv = 0;
        if((rv = cgroup_init()) != 0) {
            throw std::system_error(rv, error::cgroup_category(), "unable to initialize cgroups");
        }
#endif
        repository.insert<isolate::process_t>("process");
        repository.insert<node_t>("node::v2");
    }
}
