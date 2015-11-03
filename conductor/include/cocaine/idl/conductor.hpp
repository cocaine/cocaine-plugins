
#ifndef COCAINE_CONDUCTOR_SERVICE_INTERFACE_HPP
#define COCAINE_CONDUCTOR_SERVICE_INTERFACE_HPP

#include <cocaine/traits/dynamic.hpp>

#include <cocaine/dynamic.hpp>
#include <cocaine/rpc/protocol.hpp>
#include <cocaine/idl/primitive.hpp>


#include <boost/mpl/list.hpp>
#include <vector>

namespace cocaine { namespace io {

struct conductor_tag;

struct conductor {

    struct spool {
        typedef conductor_tag tag;

        static const char* alias() {
            return "spool";
        }

        typedef boost::mpl::list<
            uint64_t, // request_id
            std::string, // application name
            dynamic_t // profile
        >::type argument_type;

        typedef option_of<
            dynamic_t
        >::tag upstream_type;
    };

    struct spawn {
        typedef conductor_tag tag;

        static const char* alias() {
            return "spawn";
        }

        typedef boost::mpl::list<
            uint64_t, // request_id
            std::string, // application name
            dynamic_t, // profile
            dynamic_t, // arguments
            dynamic_t // environment
        >::type argument_type;

        typedef option_of<
            dynamic_t // container_id
        >::tag upstream_type;
    };

    struct terminate {
        typedef conductor_tag tag;

        static const char* alias() {
            return "terminate";
        }

        typedef boost::mpl::list<
            uint64_t, // request_id
            std::string // container_id
        > argument_type;

        typedef option_of<
            dynamic_t // container_id
        >::tag upstream_type;
    };

    struct cancel {
        typedef conductor_tag tag;

        static const char* alias() {
            return "cancel";
        }

        typedef boost::mpl::list<
            uint64_t // request_id
        >::type argument_type;

        typedef void upstream_type;
    };
};

template<>
struct protocol<conductor_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        conductor::spool,
        conductor::spawn,
        conductor::terminate,
        conductor::cancel
    > messages;

    typedef conductor type;
};

}} // namespace cocaine::io

#endif
