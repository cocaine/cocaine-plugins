#pragma once

#include <cstddef>
#include <string>
#include <system_error>

/// Intentionally left header-only.

namespace cocaine {
namespace service {
namespace node {

enum worker_errors {
    event_not_found = 1
};

/// Represents a worker internal error category.
///
/// This type of category if used for all worker errors that **can** be enumerated explicitly.
struct worker_category_t : public std::error_category {
    constexpr static auto id() -> std::size_t {
        return 42;
    }

    auto name() const noexcept -> const char* {
        return "worker category";
    }

    auto message(int ec) const noexcept -> std::string {
        switch (ec) {
        case event_not_found:
            return "event not found";
        default:
            ;
        }

        return std::string(name()) + ": " + std::to_string(ec);
    }
};

/// Represents an userland error category.
///
/// This type of category is used for all errors that is caused by a worker event handlers and
/// **cannot** be enumerated explicitly.
struct worker_user_category_t : public std::error_category {
    constexpr static auto id() -> std::size_t {
        return 43;
    }

    auto name() const noexcept -> const char* {
        return "worker user category";
    }

    auto message(int ec) const noexcept -> std::string {
        return std::string(name()) + ": " + std::to_string(ec);
    }
};

inline auto worker_category() -> const std::error_category& {
    static worker_category_t category;
    return category;
}

inline auto worker_user_category() -> const std::error_category& {
    static worker_user_category_t category;
    return category;
}

inline auto make_error_code(worker_errors err) -> std::error_code {
    return std::error_code(static_cast<int>(err), worker_category());
}

}  // namespace node
}  // namespace service
}  // namespace cocaine

namespace std {

/// Extends the type trait std::is_error_code_enum to identify `worker_internal_errors` error codes.
template<>
struct is_error_code_enum<cocaine::service::node::worker_errors> : public true_type {};

}  // namespace std
