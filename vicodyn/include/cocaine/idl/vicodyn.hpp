#pragma once

#include <cocaine/rpc/protocol.hpp>

namespace cocaine {
namespace io {

struct vicodyn_tag;

struct vicodyn {
    struct info {
        typedef vicodyn_tag tag;

        static constexpr auto alias() -> const char* {
            return "info";
        }

        typedef boost::mpl::list<>::type argument_type;

        typedef option_of<
            dynamic_t
        >::tag upstream_type;
    };

    struct peers {
        typedef vicodyn_tag tag;

        static constexpr auto alias() -> const char* {
            return "peers";
        }

        typedef boost::mpl::list<optional<std::string>>::type argument_type;

        typedef option_of<
            dynamic_t
        >::tag upstream_type;
    };

    struct apps {
        typedef vicodyn_tag tag;

        static constexpr auto alias() -> const char* {
            return "apps";
        }

        typedef boost::mpl::list<optional<std::string>>::type argument_type;

        typedef option_of<
            dynamic_t
        >::tag upstream_type;
    };
};

template<>
struct protocol<vicodyn_tag> {
    typedef vicodyn type;

    typedef boost::mpl::int_<1>::type version;

    typedef boost::mpl::list<
        vicodyn::info,
        vicodyn::peers,
        vicodyn::apps
    >::type messages;
};

} // namespace io
} // namespace cocaine
