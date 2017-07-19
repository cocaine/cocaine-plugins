#include <cocaine/errors.hpp>

#include "cocaine/idl/unicat.hpp"

#include "backend.hpp"

namespace cocaine { namespace unicat {

backend_t::backend_t(const options_t& options) :
    options(options)
{}

auto backend_t::logger() -> std::shared_ptr<logging::logger_t> {
    return get_options().log;
}

auto backend_t::get_options() -> options_t {
    return options;
}

}
}
