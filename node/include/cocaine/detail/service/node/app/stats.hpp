#pragma once

#include <atomic>
#include <cstdint>

#include <boost/accumulators/framework/accumulator_set.hpp>
#include <boost/accumulators/statistics.hpp>

#include <cocaine/locked_ptr.hpp>

namespace cocaine { // namespace detail { namespace service { namespace node { namespace app {

struct stats_t {
    /// Channel processing time quantiles (summary).
    typedef boost::accumulators::accumulator_set<
        double,
        boost::accumulators::stats<
            boost::accumulators::tag::extended_p_square
        >
    > quantiles_t;

    synchronized<quantiles_t> timings;

    stats_t();

    struct quantile_t {
        double probability;
        double value;
    };

    std::vector<quantile_t>
    quantiles() const;

private:
    const std::vector<double>&
    probabilities() const noexcept;
};

} //}}}} // namespace cocaine::detail::service::node::app
