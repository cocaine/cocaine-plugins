#pragma once

#include <blackhole/logger.hpp>
namespace cocaine {

class elliptics_logger_t: public blackhole::logger_t {
    std::unique_ptr<blackhole::logger_t> inner;

public:
    struct scope_t {
        scope_t() {
            prev_trace = trace_t::current();
            // We intentionally zero everything, as elliptics client writes itself trace_id attribute
            trace_t::current() = trace_t(0, 0, 0, dnet_logger_get_trace_bit(), {});
        }
        ~scope_t() {
            trace_t::current() = prev_trace;
        }
        trace_t prev_trace;
    };

    elliptics_logger_t(std::unique_ptr<blackhole::logger_t> log):
        inner(std::move(log))
    {}

    auto map_severity(blackhole::severity_t severity) -> blackhole::severity_t {
        if(severity) {
            severity = severity - 1;
        }
        return severity;
    }

    auto log(blackhole::severity_t severity, const blackhole::message_t& message) -> void {
        scope_t scope;
        return inner->log(map_severity(severity), message);
    }
    auto log(blackhole::severity_t severity, const blackhole::message_t& message, blackhole::attribute_pack& pack) -> void {
        scope_t scope;
        return inner->log(map_severity(severity), message, pack);
    }
    auto log(blackhole::severity_t severity, const blackhole::lazy_message_t& message, blackhole::attribute_pack& pack) -> void {
        scope_t scope;
        return inner->log(map_severity(severity), message, pack);
    }

    auto manager() -> blackhole::scope::manager_t& {
        scope_t scope;
        return inner->manager();
    }
};

} // namespace cocaine
