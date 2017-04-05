#pragma once

#include <metrics/accumulator/sliding/window.hpp>
#include <metrics/accumulator/decaying/exponentially.hpp>
#include <metrics/accumulator/snapshot/uniform.hpp>
#include <metrics/meter.hpp>
#include <metrics/timer.hpp>
#include <metrics/visitor.hpp>

namespace cocaine {
namespace service {
namespace metrics {

class plain_t : public libmetrics::visitor_t {
    const std::string m_name;
    dynamic_t::object_t& m_out;

public:
    plain_t(const std::string& name, dynamic_t::object_t& out) :
        m_name(name),
        m_out(out)
    {}

    auto visit(const libmetrics::gauge<std::int64_t>& metric) -> void override {
        do_visit(metric);
    }

    auto visit(const libmetrics::gauge<std::uint64_t>& metric) -> void override {
        do_visit(metric);
    }

    auto visit(const libmetrics::gauge<std::double_t>& metric) -> void override {
        do_visit(metric);
    }

    auto visit(const libmetrics::gauge<std::string>& metric) -> void override {
        do_visit(metric);
    }

    auto visit(const std::atomic<std::int64_t>& metric) -> void override {
        do_visit(metric);
    }

    auto visit(const std::atomic<std::uint64_t>& metric) -> void override {
        do_visit(metric);
    }

    auto visit(const libmetrics::meter_t& metric) -> void override {
        m_out[m_name + ".count"] = metric.count();
        m_out[m_name + ".m01rate"] = metric.m01rate();
        m_out[m_name + ".m05rate"] = metric.m05rate();
        m_out[m_name + ".m15rate"] = metric.m15rate();
    }

    auto visit(const libmetrics::timer<libmetrics::accumulator::sliding::window_t>& metric) -> void override {
        do_visit(metric);
    }

    auto visit(const libmetrics::timer<libmetrics::accumulator::decaying::exponentially_t>& metric) -> void override {
        do_visit(metric);
    }

private:
    template<typename T>
    auto do_visit(const libmetrics::gauge<T>& metric) -> void {
        m_out[m_name] = metric();
    }

    template<typename T>
    auto do_visit(const std::atomic<T>& metric) -> void {
        m_out[m_name] = metric.load();
    }

    template<typename T>
    auto do_visit(const libmetrics::timer<T>& metric) -> void {
        m_out[m_name + ".count"] = metric.count();
        m_out[m_name + ".m01rate"] = metric.m01rate();
        m_out[m_name + ".m05rate"] = metric.m05rate();
        m_out[m_name + ".m15rate"] = metric.m15rate();

        const auto snapshot = metric.snapshot();
        m_out[m_name + ".p50"] = snapshot.median() / 1e6;
        m_out[m_name + ".p75"] = snapshot.p75() / 1e6;
        m_out[m_name + ".p90"] = snapshot.p90() / 1e6;
        m_out[m_name + ".p95"] = snapshot.p95() / 1e6;
        m_out[m_name + ".p98"] = snapshot.p98() / 1e6;
        m_out[m_name + ".p99"] = snapshot.p99() / 1e6;
        m_out[m_name + ".mean"] = snapshot.mean() / 1e6;
        m_out[m_name + ".stddev"] = snapshot.stddev() / 1e6;
    }
};

}  // namespace metrics
}  // namespace service
}  // namespace cocaine
