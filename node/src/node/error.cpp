#include "cocaine/idl/node.hpp"

namespace {

// Node Service errors

struct node_category_t:
    public std::error_category
{
    virtual
    const char*
    name() const noexcept {
        return "node category";
    }

    virtual
    std::string
    message(int ec) const {
        using cocaine::error::node_errors;

        switch (ec) {
        case node_errors::deadline_error:
            return "invocation deadline has passed";
        case node_errors::resource_error:
            return "no resources available to complete invocation";
        case node_errors::timeout_error:
            return "invocation has timed out";
        case node_errors::invalid_assignment:
            return "failed to assign event to a slave";
        case node_errors::already_started:
            return "application has already been started";
        case node_errors::not_running:
            return "application is not running";
        case node_errors::uncaught_spool_error:
            return "uncaught error while spooling app";
        case node_errors::uncaught_publish_error:
            return "uncaught error while publishing app";
        default:
            break;
        }

        return "unknown node error " + std::to_string(ec);
    }
};

} // namespace

namespace cocaine { namespace error {

const std::error_category&
node_category() {
    static node_category_t category;
    return category;
}

std::error_code
make_error_code(node_errors code) {
    return std::error_code(static_cast<int>(code), node_category());
}

}} // namespace cocaine::error
