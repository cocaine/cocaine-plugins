#include "cocaine/vicodyn/access_log.hpp"

#include <cocaine/format.hpp>
#include <cocaine/format/error_code.hpp>
#include <cocaine/logging.hpp>

#include <blackhole/wrapper.hpp>
namespace cocaine {
namespace vicodyn {

access_log_t::access_log_t(blackhole::logger_t& logger) :
    logger(logger),
    start_time(clock_t::now()),
    attributes(),
    closed(ATOMIC_FLAG_INIT)
{}

access_log_t::~access_log_t() {
    static std::string msg("finished request (by access_log dtor)");
    write(logging::info, msg);
}

auto access_log_t::add(blackhole::attribute_t attribute) -> void {
    attributes->push_back(std::move(attribute));
}

auto access_log_t::add_checkpoint(std::string name) -> void {
    attributes->emplace_back(std::move(name), format("{} ms", current_duration_ms()));
}

auto access_log_t::finish() -> void {
    static std::string msg("finished request");
    write(logging::info, msg);
}

auto access_log_t::fail(const std::error_code& ec, blackhole::string_view reason) -> void {
    write(logging::warning, format("finished request with error {} - {}", ec, reason));
}

auto access_log_t::current_duration_ms() -> size_t {
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(clock_t::now() - start_time);
    return dur.count();
}

auto access_log_t::write(int level, const std::string& msg) -> void {
    if(closed.test_and_set()) {
        return;
    }
    static std::string d("total_duration");
    add_checkpoint(d);

    blackhole::view_of<blackhole::attributes_t>::type view(attributes.unsafe().begin(), attributes.unsafe().end());

    COCAINE_LOG(logger, logging::priorities(level), msg, view);
}

} // namespace vicodyn
} // namespace cocaine
