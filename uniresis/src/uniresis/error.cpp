#include "cocaine/uniresis/error.hpp"

#include <cocaine/format.hpp>

namespace cocaine {
namespace uniresis {

class uniresis_category_t : public std::error_category {
public:
    auto name() const noexcept -> const char* {
        return "uniresis category";
    }

    auto message(int code) const -> std::string {
        switch (static_cast<uniresis_errc>(code)) {
        case uniresis_errc::failed_calculate_cpu_count:
            return "failed to calculate total CPU count";
        case uniresis_errc::failed_calculate_system_memory:
            return "failed to calculate the total system memort on a machine";
        }

        return format("{}: {}", name(), code);
    }
};

auto uniresis_category() -> const std::error_category& {
    static uniresis_category_t category;
    return category;
}

auto make_error_code(uniresis_errc err) -> std::error_code {
    return std::error_code(static_cast<int>(err), uniresis_category());
}

}  // namespace tvm
}  // namespace cocaine
