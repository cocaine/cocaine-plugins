#pragma once

#include <boost/mpl/list.hpp>

namespace cocaine { namespace io {

struct elasticsearch_tag;

namespace elasticsearch {

struct get {
    typedef elasticsearch_tag tag;

    typedef boost::mpl::list<
        std::string
    > tuple_type;

    typedef std::string result_type;
};

struct index {
    typedef elasticsearch_tag tag;

    typedef boost::mpl::list<
        /* index */ std::string,
        /* data */ std::string
    > tuple_type;

    typedef boost::mpl::list<
        /* id */ std::string
    >result_type;
};

}

template<>
struct protocol<elasticsearch_tag> {
    typedef mpl::list<
        elasticsearch::get,
        elasticsearch::index
    > type;

    typedef boost::mpl::int_<
        1
    >::type version;
};

} }
