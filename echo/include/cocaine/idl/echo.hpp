#pragma once

#include <cocaine/rpc/protocol.hpp>

namespace cocaine {
namespace io {

struct echo_tag;

struct echo {
    struct ping {
        typedef echo_tag tag;

        static constexpr const char* alias() noexcept {
            return "ping";
        }

        typedef boost::mpl::list<std::string>::type argument_type;
        typedef option_of<std::string>::tag upstream_type;
    };
};

template <>
struct protocol<echo_tag> {
    typedef echo type;

    typedef boost::mpl::int_<1>::type version;
    typedef boost::mpl::list<echo::ping>::type messages;
};

}  // namespace io
}  // namespace cocaine
