#include "cocaine/error/postgres.hpp"

namespace cocaine {
namespace error {

const std::error_category&
postgres_category() {
    static postgres_category_t instance;
    return instance;
}

std::error_code
make_error_code(postgres_errors err) {
    return std::error_code(static_cast<int>(err), postgres_category());
}

} // namespace error
} // namespace cocaine
