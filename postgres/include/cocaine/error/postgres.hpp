#pragma once

#include <cstddef>
#include <string>
#include <system_error>

namespace cocaine {
namespace error {

enum postgres_errors {
    unknown_pg_error = 1
};

struct postgres_category_t : public std::error_category {
    constexpr static size_t
    id() {
        return 0x40BC;
    }

    const char*
    name() const noexcept {
        return "postgres category";
    }

    std::string
    message(int ec) const noexcept {
        switch (ec) {
            case unknown_pg_error:
                return "postgres error";
            default:
                return std::string(name()) + ": " + std::to_string(ec);
        }
    }
};

std::error_code make_error_code(postgres_errors err);

}  // namespace error
}  // namespace cocaine

namespace std {

/// Extends the type trait std::is_error_code_enum to identify `postgres_errors` error codes.
template<>
struct is_error_code_enum<cocaine::error::postgres_errors> : public true_type {};

}  // namespace std
