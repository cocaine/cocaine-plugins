#pragma once

#include <string>
#include <system_error>

namespace cocaine {
namespace uniresis {

enum class uniresis_errc {
    /// Failed to calculate the number of cores on a machine.
    failed_calculate_cpu_count,
    /// Failed to calculate the total system memort on a machine.
    failed_calculate_system_memory,
};

auto uniresis_category() -> const std::error_category&;

auto make_error_code(uniresis_errc err) -> std::error_code;

} // namespace uniresis
} // namespace cocaine

namespace std {

/// Extends the type trait `std::is_error_code_enum` to identify `uniresis_errc` error codes.
template<>
struct is_error_code_enum<cocaine::uniresis::uniresis_errc> : public true_type {};

/// Extends the type trait `std::is_error_condition_enum` to identify `uniresis_errc` error
/// conditions.
template<>
struct is_error_condition_enum<cocaine::uniresis::uniresis_errc> : public true_type {};

} // namespace std
