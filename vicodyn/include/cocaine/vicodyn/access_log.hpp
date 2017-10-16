#pragma once

#include <blackhole/attributes.hpp>
#include <blackhole/logger.hpp>

#include <atomic>
#include <chrono>
#include <system_error>

namespace cocaine {
namespace vicodyn {

class access_log_t {
public:
    using clock_t = std::chrono::system_clock;
    access_log_t(blackhole::logger_t& logger);
    ~access_log_t();

    auto add(blackhole::attribute_t attribute) -> void;
    auto add_checkpoint(std::string name) -> void;

    auto finish() -> void;

    auto fail(const std::error_code& ec, blackhole::string_view reason) -> void;

private:
    auto current_duration_ms() -> size_t;
    auto write(int level, const std::string& msg) -> void;

    blackhole::logger_t& logger;
    clock_t::time_point start_time;
    blackhole::attributes_t attributes;
    std::atomic_flag closed;
};

} // namespace vicodyn
} // namespace cocaine
