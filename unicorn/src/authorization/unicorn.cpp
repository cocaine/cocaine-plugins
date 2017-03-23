#include "unicorn.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/numeric.hpp>

#include <blackhole/wrapper.hpp>

#include <cocaine/api/unicorn.hpp>
#include <cocaine/context.hpp>
#include <cocaine/dynamic.hpp>
#include <cocaine/dynamic/converters.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/executor/asio.hpp>
#include <cocaine/format.hpp>
#include <cocaine/format/vector.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/memory.hpp>
#include <cocaine/traits/enum.hpp>
#include <cocaine/traits/map.hpp>
#include <cocaine/traits/tuple.hpp>
#include <cocaine/unicorn/value.hpp>

#include "cocaine/idl/unicorn.hpp"

namespace cocaine {

template<>
struct dynamic_converter<authorization::unicorn::metainfo_t> {
    using result_type = authorization::unicorn::metainfo_t;

    using underlying_type = std::tuple<
        std::map<auth::cid_t, authorization::unicorn::flags_t>,
        std::map<auth::uid_t, authorization::unicorn::flags_t>
    >;

    static
    result_type
    convert(const dynamic_t& from) {
        auto& tuple = from.as_array();
        if (tuple.size() != 2) {
            throw std::bad_cast();
        }

        return result_type{
            dynamic_converter::convert<auth::cid_t, authorization::unicorn::flags_t>(tuple.at(0)),
            dynamic_converter::convert<auth::uid_t, authorization::unicorn::flags_t>(tuple.at(1)),
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
    std::map<K, authorization::unicorn::flags_t>
    convert(const dynamic_t& from) {
        std::map<K, authorization::unicorn::flags_t> result;
        const dynamic_t::object_t& object = from.as_object();

        for(auto it = object.begin(); it != object.end(); ++it) {
            result.insert(std::make_pair(boost::lexical_cast<K>(it->first), it->second.to<T>()));
        }

        return result;
    }
};

} // namespace cocaine

namespace cocaine { namespace io {

template<>
struct type_traits<authorization::unicorn::metainfo_t> {
    typedef authorization::unicorn::metainfo_t type;
    typedef boost::mpl::list<
        std::map<auth::cid_t, authorization::unicorn::flags_t>,
        std::map<auth::uid_t, authorization::unicorn::flags_t>
    > underlying_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const type& source) {
        type_traits<underlying_type>::pack(target, source.c_perms, source.u_perms);
    }

    static inline
    void
    unpack(const msgpack::object& source, type& target) {
        type_traits<underlying_type>::unpack(source, target.c_perms, target.u_perms);
    }
};

}} // namespace cocaine::io

namespace cocaine {
namespace authorization {
namespace unicorn {

using cocaine::unicorn::value_t;
using cocaine::unicorn::versioned_value_t;

namespace {

struct operation_t {
    flags_t flag;

    static
    auto
    from(std::size_t event) -> operation_t {
        switch (event) {
        case io::event_traits<io::unicorn::create>::id:
        case io::event_traits<io::unicorn::put>::id:
        case io::event_traits<io::unicorn::remove>::id:
        case io::event_traits<io::unicorn::del>::id:
        case io::event_traits<io::unicorn::increment>::id:
        case io::event_traits<io::unicorn::lock>::id:
            return {flags_t::both};
        case io::event_traits<io::unicorn::get>::id:
        case io::event_traits<io::unicorn::subscribe>::id:
        case io::event_traits<io::unicorn::children_subscribe>::id:
            return {flags_t::read};
        default:
            __builtin_unreachable();
        }
    }

    auto
    is_read() const -> bool {
        return (flag & flags_t::read) == flags_t::read;
    }

    auto
    is_modify() const -> bool {
        return (flag & flags_t::both) == flags_t::both;
    }
};

auto
extract_prefix(const std::string& path) -> boost::optional<std::string> {
    std::vector<std::string> paths;
    boost::split(paths, path, boost::is_any_of("/"), boost::token_compress_on);
    if (paths.empty() || paths.front().empty()) {
        return boost::none;
    } else {
        return paths.front();
    }
}

template<typename S, typename P>
auto
extract_permissions(const S& subjects, P& perms) -> std::size_t {
    using value_type = typename S::value_type;

    if (subjects.empty()) {
        return flags_t::none;
    }

    return boost::accumulate(
        subjects,
        static_cast<std::size_t>(flags_t::both),
        [&](std::size_t acc, value_type id) {
            return acc & perms[id];
        }
    );
}

} // namespace

enabled_t::enabled_t(context_t& context, const std::string& service, const dynamic_t& args) :
    log(context.log(cocaine::format("authorization/{}/unicorn", service))),
    backend(api::unicorn(context, args.as_object().at("backend", "core").as_string())),
    executor(std::make_unique<executor::owning_asio_t>()),
    counter(0)
{}

auto
enabled_t::verify(std::size_t event, const std::string& path, const auth::identity_t& identity, callback_type callback)
    -> void
{
    if (path.empty() || path.front() != '/') {
        callback(make_error_code(error::invalid_path));
        return;
    }

    const auto prefix = extract_prefix(path.substr(1));
    if (prefix == boost::none) {
        callback(make_error_code(error::invalid_path));
        return;
    }

    // Operation id.
    const auto id = ++counter;

    auto cids = identity.cids();
    auto uids = identity.uids();
    auto log_ = std::make_shared<blackhole::wrapper_t>(*log, blackhole::attributes_t{
        {"id", id},
        {"path", path},
        {"event", event},
        {"prefix", *prefix},
        {"cids", cocaine::format("{}", cids)},
        {"uids", cocaine::format("{}", uids)},
    });

    // Cleans up scopes and invokes the callback.
    auto finalize = [=](std::error_code ec) {
        scopes.apply([&](std::multimap<std::uint64_t, std::shared_ptr<api::unicorn_scope_t>>& scopes_) {
            COCAINE_LOG_DEBUG(log_, "completed ACL lookup/update operation(s)", {
                {"code", ec.value()},
                {"scopes", scopes_.count(id)},
                {"message", ec ? ec.message() : "no error"},
            });
            scopes_.erase(id);
        });

        callback(ec);
    };

    auto lookup_permissions = [=](versioned_value_t value) {
        COCAINE_LOG_DEBUG(log_, "ACL metainfo: {}", boost::lexical_cast<std::string>(value.value()));

        metainfo_t metainfo;
        if (value.exists()) {
            if (!value.value().convertible_to<metainfo_t>()) {
                throw std::system_error(make_error_code(error::invalid_acl_framing));
            }

            metainfo = value.value().to<metainfo_t>();
        }

        COCAINE_LOG_DEBUG(log_, "looked up ACL metainfo: {} records, exists: {}", metainfo.size(), value.exists());

        const auto op = operation_t::from(event);

        if (metainfo.empty()) {
            if (op.is_modify() && (cids.size() > 0 || uids.size() > 0)) {
                COCAINE_LOG_INFO(log_, "initializing ACL for '{}' prefix for cid(s) and uid(s)", *prefix);

                for (auto cid : cids) {
                    metainfo.c_perms.insert(std::make_pair(cid, flags_t::both));
                }
                for (auto uid : uids) {
                    metainfo.u_perms.insert(std::make_pair(uid, flags_t::both));
                }

                COCAINE_LOG_DEBUG(log_, "starting ACL update operation");
                scopes.apply([&](std::multimap<std::uint64_t, std::shared_ptr<api::unicorn_scope_t>>& scopes_) {
                    scopes_.insert({
                        id,
                        upload_permissions(*prefix, metainfo, value, finalize)
                    });
                });
            } else {
                // Allow anonymous read-only access for non-owning prefixes.
                finalize({});
            }
            return;
        }

        BOOST_ASSERT(!metainfo.empty());

        auto c_perm = extract_permissions(cids, metainfo.c_perms);
        auto u_perm = extract_permissions(uids, metainfo.u_perms);
        auto allowed = ((c_perm | u_perm) & op.flag) == op.flag;

        if (!allowed) {
            throw std::system_error(std::make_error_code(std::errc::permission_denied));
        }

        finalize({});
    };

    scopes.apply([&](std::multimap<std::uint64_t, std::shared_ptr<api::unicorn_scope_t>>& scopes_) {
        COCAINE_LOG_DEBUG(log_, "starting ACL lookup operation");
        auto scope = backend->get([=](std::future<versioned_value_t> future) mutable {
            try {
                lookup_permissions(future.get());
            } catch(const std::system_error& err) {
                executor->spawn([=] {
                    finalize(err.code());
                });
            }
        }, make_path(*prefix));

        scopes_.insert({id, std::move(scope)});
    });
}

auto
enabled_t::make_path(const std::string& prefix) const -> std::string {
    return cocaine::format("/.acls/{}", prefix);
}

auto
enabled_t::upload_permissions(const std::string& prefix, metainfo_t metainfo, const versioned_value_t& value, callback_type callback)
    -> std::shared_ptr<api::unicorn_scope_t>
{
    if (value.exists()) {
        return update_permissions(prefix, std::move(metainfo), value.version(), std::move(callback));
    } else {
        return create_permissions(prefix, std::move(metainfo), std::move(callback));
    }
}

auto
enabled_t::create_permissions(const std::string& prefix, metainfo_t metainfo, callback_type callback)
    -> std::shared_ptr<api::unicorn_scope_t>
{
    dynamic_t value = std::make_tuple(
        std::move(metainfo.c_perms),
        std::move(metainfo.u_perms)
    );
    return backend->create([=](std::future<bool> future) mutable {
        try {
            const auto updated = future.get();
            if (updated) {
                callback({});
            } else {
                throw std::system_error(make_error_code(error::permissions_changed));
            }
        } catch (const std::system_error& err) {
            callback(err.code());
        }
    }, make_path(prefix), value_t(std::move(value)), false, false);
}

auto
enabled_t::update_permissions(const std::string& prefix, metainfo_t metainfo, std::size_t version, callback_type callback)
    -> std::shared_ptr<api::unicorn_scope_t>
{
    dynamic_t value = std::make_tuple(
        std::move(metainfo.c_perms),
        std::move(metainfo.u_perms)
    );

    return backend->put([=](std::future<std::tuple<bool, versioned_value_t>> future) mutable {
        try {
            bool updated;
            std::tie(updated, std::ignore) = future.get();
            if (updated) {
                callback({});
            } else {
                throw std::system_error(make_error_code(error::permissions_changed));
            }
        } catch (const std::system_error& err) {
            callback(err.code());
        }
    }, make_path(prefix), value_t(std::move(value)), version);
}

} // namespace unicorn
} // namespace authorization
} // namespace cocaine
