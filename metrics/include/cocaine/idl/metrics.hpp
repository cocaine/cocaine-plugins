#pragma once

#include <cocaine/rpc/protocol.hpp>

namespace cocaine {
namespace io {

struct metrics_tag;

struct metrics {

struct metrics_base {
    typedef boost::mpl::list<
        std::map<std::string, std::string>
    >::type argument_type;
};

struct counter_get : public metrics_base {
    typedef metrics_tag tag;

    static const char* alias() {
        return "counter_get";
    }

    typedef metrics_base::argument_type argument_type;

    typedef option_of<
        std::int64_t
    >::tag upstream_type;
};

struct timer_get : public metrics_base {
    typedef metrics_tag tag;

    static const char* alias() {
        return "timer_get";
    }

    typedef metrics_base::argument_type argument_type;

    typedef option_of<
        std::uint64_t, // Count.
        double,        // EWMA for the last 1-munute.
        double,        // EWMA for the last 5-minutes.
        double,        // EWMA for the last 15-minutes.
        double,        // 50 percentile (median).
        double,        // 75 percentile
        double,        // 90 percentile.
        double,        // 95 percentile.
        double,        // 98 percentile.
        double         // 99 percentile.
    >::tag upstream_type;
};

};

template<>
struct protocol<metrics_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        metrics::counter_get,
        metrics::timer_get
    >::type messages;

    typedef metrics scope;
};

}  // namespace io
}  // namespace cocaine
