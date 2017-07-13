#pragma once

#include <map>
#include <vector>
#include <iostream>

#include <stdexcept>

#include <cocaine/auth/uid.hpp>
#include <cocaine/format.hpp>
#include <cocaine/dynamic.hpp>
#include <cocaine/dynamic/converters.hpp>

#include "cocaine/traits/enum.hpp"
#include "cocaine/traits/map.hpp"
#include "cocaine/traits/tuple.hpp"

#include <boost/mpl/list.hpp>

// TODO: taken from private core, should be moved into 'core' source tree as
//       public interface someday
namespace cocaine { namespace auth {

    // TODO: Would be CRUD permissions more approppriate and generic?
    enum flags_t : std::size_t { none = 0x00, read = 0x01, write = 0x02, both = read | write };

    struct metainfo_t {
        std::map<auth::cid_t, flags_t> c_perms;
        std::map<auth::uid_t, flags_t> u_perms;

        auto
        empty() const -> bool {
            return c_perms.empty() && u_perms.empty();
        }
    };

    auto operator<<(std::ostream& os, const metainfo_t& meta) -> std::ostream&;

    struct alter_data_t {
        std::vector<auth::cid_t> cids;
        std::vector<auth::uid_t> uids;
        auth::flags_t flags;

        auto
        make_identity() -> auth::identity_t {
            return auth::identity_t::builder_t().cids(cids).uids(uids).build();
        }
    };

    template<typename Event>
    auto
    alter(auth::metainfo_t& metainfo, const alter_data_t& data) -> void;
} // auth
} // cocaine

namespace cocaine {
namespace io {

template<>
struct type_traits<auth::metainfo_t> {
    typedef boost::mpl::list<
        std::map<auth::cid_t, auth::flags_t>,
        std::map<auth::uid_t, auth::flags_t>
    > underlying_type;

    template<class Stream>
    static
    void
    pack(msgpack::packer<Stream>& target, const auth::metainfo_t& meta) {
        type_traits<underlying_type>::pack(target, meta.c_perms, meta.u_perms);
    }

    static
    void
    unpack(const msgpack::object& source, auth::metainfo_t& target) {
        type_traits<underlying_type>::unpack(source, target.c_perms, target.u_perms);
    }
};

} // namespace io
} // namespace cocaine

// Namespace section copy-pasted form uncorn plugin:
// unicorn/src/authorization/unicorn.cpp
namespace cocaine {

template<>
struct dynamic_converter<auth::metainfo_t> {
    using result_type = auth::metainfo_t;

    using underlying_type = std::tuple<
        std::map<auth::cid_t, auth::flags_t>,
        std::map<auth::uid_t, auth::flags_t>
    >;

    static
    result_type
    convert(const dynamic_t& from) {
        auto& tuple = from.as_array();
        if (tuple.size() != 2) {
            throw std::invalid_argument{
                cocaine::format("wrong number of ACL records, should be 2, but {} was provided", tuple.size())};
        }

        return result_type{
            dynamic_converter::convert<auth::cid_t, auth::flags_t>(tuple.at(0)),
            dynamic_converter::convert<auth::uid_t, auth::flags_t>(tuple.at(1)),
        };
    }

    static
    bool
    convertible(const dynamic_t& from) {
        return from.is_array() && from.as_array().size() == 2;
    }

private:
    // TODO: Temporary until `dynamic_t` teaches how to convert into maps with non-string keys.
    template<typename K, typename T>
    static
    std::map<K, auth::flags_t>
    convert(const dynamic_t& from) {
        std::map<K, auth::flags_t> result;
        const dynamic_t::object_t& object = from.as_object();

        for(auto it = object.begin(); it != object.end(); ++it) {
            result.insert(std::make_pair(boost::lexical_cast<K>(it->first), it->second.to<T>()));
        }

        return result;
    }
};

} // namespace cocaine
