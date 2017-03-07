#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include <metrics/accumulator/sliding/window.hpp>
#include <metrics/accumulator/decaying/exponentially.hpp>
#include <metrics/accumulator/snapshot/uniform.hpp>
#include <metrics/meter.hpp>
#include <metrics/timer.hpp>
#include <metrics/visitor.hpp>

#include <cocaine/dynamic.hpp>

namespace cocaine {
namespace service {
namespace metrics {

class dendroid_t : public libmetrics::visitor_t {
    std::vector<std::string> m_parts;
    std::reference_wrapper<dynamic_t::object_t> m_out;

public:
    dendroid_t(const std::string& name, dynamic_t::object_t& out) :
        m_out(out)
    {
        boost::split(m_parts, name, boost::is_any_of("."), boost::token_compress_on);
        if (m_parts.empty()) {
            throw cocaine::error_t("metric name must contain at least one alphanumeric character");
        }
    }

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
        traverse();
        auto& out = m_out.get();
        out["count"] = metric.count();
        out["m01rate"] = metric.m01rate();
        out["m05rate"] = metric.m05rate();
        out["m15rate"] = metric.m15rate();
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
        const auto name = m_parts.back();
        m_parts.pop_back();

        traverse();
        auto& out = m_out.get();
        out[name] = metric();
    }

    template<typename T>
    auto do_visit(const std::atomic<T>& metric) -> void {
        const auto name = m_parts.back();
        m_parts.pop_back();

        traverse();
        auto& out = m_out.get();
        out[name] = metric.load();
    }

    template<typename T>
    auto do_visit(const libmetrics::timer<T>& metric) -> void {
        traverse();
        auto& out = m_out.get();
        out["count"] = metric.count();
        out["m01rate"] = metric.m01rate();
        out["m05rate"] = metric.m05rate();
        out["m15rate"] = metric.m15rate();

        const auto snapshot = metric.snapshot();
        out["p50"] = snapshot.median() / 1e6;
        out["p75"] = snapshot.p75() / 1e6;
        out["p90"] = snapshot.p90() / 1e6;
        out["p95"] = snapshot.p95() / 1e6;
        out["p98"] = snapshot.p98() / 1e6;
        out["p99"] = snapshot.p99() / 1e6;
        out["mean"] = snapshot.mean() / 1e6;
        out["stddev"] = snapshot.stddev() / 1e6;
    }

private:
    auto
    traverse() -> void {
        for (const auto& part : m_parts) {
            if (!m_out.get()[part].is_object()) {
                m_out.get()[part] = dynamic_t::object_t();
            }
            m_out = m_out.get()[part].as_object();
        }
    }
};

}  // namespace metrics
}  // namespace service
}  // namespace cocaine
