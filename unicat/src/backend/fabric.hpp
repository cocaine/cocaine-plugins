#pragma once

#include <memory>
#include <vector>
#include <unordered_map>

#include "backend.hpp"

namespace cocaine { namespace unicat {

enum class scheme_t : unsigned {
    storage,
    unicorn,
    schemes_count
};

struct url_t {
    scheme_t scheme;
    std::string service_name;
    std::string entity;
};

scheme_t scheme_from_string(const std::string& scheme);
std::string scheme_to_string(const scheme_t);

struct fabric {
    static
    auto
    make_backend(const scheme_t scheme, const backend_t::options_t& options) -> std::shared_ptr<backend_t>;

    fabric() = delete;
    fabric(const fabric&) = delete;
    void operator=(const fabric&) = delete;
};

}
}
