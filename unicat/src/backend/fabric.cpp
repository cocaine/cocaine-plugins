#include <memory>
#include <stdexcept>

#include <cocaine/idl/unicat.hpp>
#include <cocaine/errors.hpp>

#include "fabric.hpp"

#include "storage.hpp"
#include "unicorn.hpp"

namespace cocaine { namespace unicat {

const std::unordered_map<std::string, scheme_t> scheme_names_mapping =
{
    {"storage", scheme_t::storage},
    {"unicorn", scheme_t::unicorn}
};

scheme_t scheme_from_string(const std::string& scheme) {
    return scheme_names_mapping.at(scheme);
}

std::string scheme_to_string(const scheme_t scheme) {
    switch (scheme) {
        case scheme_t::storage: return "storage";
        case scheme_t::unicorn: return "unicorn";
        default: break;
    }

    throw std::runtime_error("unknown scheme");
}

auto
fabric::make_backend(const scheme_t scheme, const backend_t::options_t& options) -> std::shared_ptr<backend_t> {
    using cocaine::error::repository_errors;
    switch (scheme) {
        case scheme_t::storage: return std::make_shared<storage_backend_t>(options);
        case scheme_t::unicorn: return std::make_shared<unicorn_backend_t>(options);
        default: break;
    };
    throw cocaine::error::error_t(repository_errors::component_not_found, "unknown auth backend scheme");
}

}
}
