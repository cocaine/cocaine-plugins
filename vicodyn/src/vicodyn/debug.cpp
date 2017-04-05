#include "cocaine/vicodyn/debug.hpp"

#if defined(VICODYN_USE_DEBUG)
auto _vicodyn_debug_logger_() -> std::unique_ptr<cocaine::logging::logger_t>& {
    static std::unique_ptr<cocaine::logging::logger_t> logger;
    return logger;
}
#endif
