#include <boost/range/algorithm/transform.hpp>

#include "cocaine/auth/metainfo.hpp"
#include "cocaine/idl/unicat.hpp"

namespace cocaine { namespace auth {
// TODO: unittest to all those stuff
namespace detail {
    using base_type = std::underlying_type<auth::flags_t>::type;

    template<typename Flags>
    auto
    flags_to_uint(Flags&& fl) -> base_type {
        return static_cast<base_type>(fl);
    }

    auto
    uint_to_flags(const base_type base) -> auth::flags_t {
        return static_cast<auth::flags_t>(base);
    }

    auth::flags_t operator|=(auth::flags_t& a, const auth::flags_t b) {
        return a = uint_to_flags(flags_to_uint(a) | flags_to_uint(b));
    }

    auth::flags_t operator|=(auth::flags_t& a, const base_type b) {
        return a = uint_to_flags(flags_to_uint(a) | b);
    }

    auth::flags_t operator&=(auth::flags_t& a, const auth::flags_t b) {
        return a = uint_to_flags(flags_to_uint(a) & flags_to_uint(b));
    }

    auth::flags_t operator&=(auth::flags_t& a, const base_type b) {
        return a = uint_to_flags(flags_to_uint(a) & b);
    }

    auth::flags_t operator~(const auth::flags_t a) {
        return uint_to_flags( ~flags_to_uint(a) );
    }

    template<typename T, typename DstMap>
    auto stringify_keys(const std::map<T, auth::flags_t>& src, DstMap& dst) -> void {
        for(const auto& el : src) {
            dst[boost::lexical_cast<std::string>(el.first)] = el.second;
        }
    }

    template<typename T, typename SrcMap>
    auto digitize_keys(const SrcMap& src, std::map<T,auth::flags_t>& dst) -> void {
        for(const auto& el : src) {
            dst[boost::lexical_cast<T>(el.first)] = el.second;
        }
    }

    template<typename Perms, typename ToSet>
    auto
    set_perms(Perms&& perms, const ToSet& to_set, const auth::flags_t flags) -> void
    {
        using namespace detail;
        for(const auto& id : to_set) {
            perms[id] |= flags;
        }
    }

    template<typename Perms, typename ToUnset>
    auto
    unset_perms(Perms&& perms, const ToUnset& to_unset, const auth::flags_t flags) -> void
    {
        using namespace detail;
        for(const auto& id : to_unset) {
            auto it = perms.find(id);
            if (it != std::end(perms)) {
                it->second &= ~ flags;
            }
        }
    }
} // detail

auto operator<<(std::ostream& os, const metainfo_t& meta) -> std::ostream&
{
   os << "cids:\n";
   for(const auto& cid : meta.c_perms) {
       os << "  " << cid.first << " : " << cid.second << '\n';
   }

   os << "uids:\n";
   for(const auto& uid : meta.u_perms) {
       os << "  " << uid.first << " : " << uid.second << '\n';
   }

   return os;
}

// TODO: check for none flags
template<>
auto
alter<io::unicat::revoke>(auth::metainfo_t& metainfo, const auth::alter_data_t& data) -> void
{
    using namespace detail;
    unset_perms(metainfo.c_perms, data.cids, data.flags);
    unset_perms(metainfo.u_perms, data.uids, data.flags);
}

template<>
auto
alter<io::unicat::grant>(auth::metainfo_t& metainfo, const auth::alter_data_t& data) -> void
{
    using namespace detail;
    set_perms(metainfo.c_perms, data.cids, data.flags);
    set_perms(metainfo.u_perms, data.uids, data.flags);
}

template
auto
alter<io::unicat::grant>(auth::metainfo_t& metainfo, const auth::alter_data_t&) -> void;

template
auto
alter<io::unicat::grant>(auth::metainfo_t& metainfo, const auth::alter_data_t&) -> void;
}
}
