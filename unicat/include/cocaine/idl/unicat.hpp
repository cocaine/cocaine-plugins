#pragma once

#include <string>
#include <vector>

#include <cstdint>

#include <cocaine/auth/uid.hpp>
#include <cocaine/rpc/protocol.hpp>

#include <boost/mpl/list.hpp>

#include "cocaine/service/unicat/auth/metainfo.hpp"

#include "boost/optional.hpp"

namespace cocaine { namespace unicat {
    using entity_type = std::tuple<std::string, boost::optional<std::string>, std::string>;
}
}

namespace cocaine {
namespace io {

struct unicat_tag;

// TODO: Additional to grant/revoke methods seem redundant, e.g. create/reset
//       could be emulated by direct service methods - put/write, etc.
struct unicat {

    // TODO: aggregate cids and uids vectors into structural type in order to
    // avoid user argument placement mistakes and to reduce arguments count.
    using common_arguments_type = boost::mpl::list<
        std::vector<cocaine::unicat::entity_type>,  // (1) entities in tuple:  (scheme, uri)
        std::vector<auth::cid_t>,                   // (2) array of client ids
        std::vector<auth::uid_t>,                   // (3) array of user ids
        auth::flags_t                               // (4) access bitmask: None - 0, R - 1, W - 2, ALL - 3,
    >::type;

    // Set specified rights in addition to existent one
    // Requires RW access to acl table(s).
    struct grant {
        typedef unicat_tag tag;

        static constexpr const char* alias() noexcept {
            return "grant";
        }

        using argument_type = common_arguments_type;
    };

    // Unset specified rights from existent one
    // Requires RW access to acl table(s).
    struct revoke {
        typedef unicat_tag tag;

        static constexpr const char* alias() noexcept {
            return "revoke";
        }

        using argument_type = common_arguments_type;
    };
};

template <>
struct protocol<unicat_tag> {
    typedef unicat type;

    typedef boost::mpl::int_<1>::type version;

    typedef boost::mpl::list<
        unicat::grant,
        unicat::revoke
    >::type messages;
};

}  // namespace io
}  // namespace cocaine
