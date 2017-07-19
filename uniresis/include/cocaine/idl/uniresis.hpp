#pragma once

#include <cocaine/rpc/protocol.hpp>

namespace cocaine {
namespace io {

struct uniresis_tag;

struct uniresis {
    /// Returns total hardware CPU count available for this host.
    ///
    /// This is used mainly for proper scheduling. Note, that it is possible to restrict available
    /// CPUs by setting less value through config. It is assumed that this value is set once on
    /// service start up and will not change during its life time.
    struct cpu_count {
        typedef uniresis_tag tag;

        static constexpr auto alias() -> const char* {
            return "cpu_count";
        }

        typedef boost::mpl::list<>::type argument_type;

        typedef option_of<
            unsigned int
        >::tag upstream_type;
    };

    /// Returns total number of memory bytes available for this host.
    struct memory_count {
        typedef uniresis_tag tag;

        static constexpr auto alias() -> const char* {
            return "memory_count";
        }

        typedef boost::mpl::list<>::type argument_type;

        typedef option_of<
            std::uint64_t
        >::tag upstream_type;
    };
};

template<>
struct protocol<uniresis_tag> {
    typedef uniresis type;

    typedef boost::mpl::int_<1>::type version;

    typedef boost::mpl::list<
        uniresis::cpu_count,
        uniresis::memory_count
    >::type messages;
};

} // namespace io
} // namespace cocaine
