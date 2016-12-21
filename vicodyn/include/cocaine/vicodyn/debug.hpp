#pragma once
#if defined(VICODYN_USE_DEBUG)
#include <iostream>
#include <cocaine/logging.hpp>
#include <blackhole/logger.hpp>
std::unique_ptr<cocaine::logging::logger_t>& _vicodyn_debug_logger_();

#define VICODYN_DEBUG(...) \
if(_vicodyn_debug_logger_()) {COCAINE_LOG_WARNING(_vicodyn_debug_logger_(), __VA_ARGS__);}

#define VICODYN_REGISTER_LOGGER(logger) _vicodyn_debug_logger_() = logger;

#else
template<class... Args>
void
_vicodyn_nowhere_(Args&&...) {}
#define VICODYN_DEBUG(...) _vicodyn_nowhere_(__VA_ARGS__);
#define VICODYN_REGISTER_LOGGER(logger) _vicodyn_nowhere_(logger);

#endif
