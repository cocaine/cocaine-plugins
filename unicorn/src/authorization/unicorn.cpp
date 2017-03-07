#include "unicorn.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include <blackhole/wrapper.hpp>

#include <cocaine/api/unicorn.hpp>
#include <cocaine/context.hpp>
#include <cocaine/dynamic.hpp>
#include <cocaine/dynamic/converters.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/executor/asio.hpp>
#include <cocaine/format.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/memory.hpp>
#include <cocaine/unicorn/value.hpp>

#include "cocaine/idl/unicorn.hpp"

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

    auto uids = identity.uids();
    auto log_ = std::make_shared<blackhole::wrapper_t>(*log, blackhole::attributes_t{
        {"id", id},
        {"path", path},
        {"event", event},
        {"prefix", *prefix},
        {"uids", cocaine::format("[{}]", boost::join(uids | boost::adaptors::transformed(static_cast<std::string(*)(auth::uid_t)>(std::to_string)), ";"))},
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
            if (op.is_modify() && uids.size() > 0) {
                COCAINE_LOG_INFO(log_, "initializing ACL for '{}' prefix for uid(s) [{}]", *prefix,
                    [&](std::ostream& stream) -> std::ostream& {
                        return stream << boost::join(uids | boost::adaptors::transformed(static_cast<std::string(*)(auth::uid_t)>(std::to_string)), ", ");
                    }
                );

                metainfo.reserve(uids.size());
                for (auto uid : uids) {
                    metainfo.push_back(std::make_tuple(uid, flags_t::both));
                }

                // Sort by UID to allow binary search.
                std::sort(
                    std::begin(metainfo),
                    std::end(metainfo),
                    [](const node_t& le, const node_t& ri) -> bool {
                        return std::get<0>(le) < std::get<0>(ri);
                    }
                );

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

        BOOST_ASSERT(metainfo.size() > 0);

        const auto allowed = std::all_of(std::begin(uids), std::end(uids), [&](auth::uid_t uid) {
            const auto it = std::find_if(
                std::begin(metainfo),
                std::end(metainfo),
                [&](const node_t& node) {
                    return std::get<0>(node) == uid;
                }
            );

            if (it == std::end(metainfo)) {
                return false;
            }

            return (std::get<1>(*it) & op.flag) == op.flag;
        });

        if (uids.empty()) {
            throw std::system_error(std::make_error_code(std::errc::permission_denied));
        }

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
    }, make_path(prefix), value_t(std::move(metainfo)), false, false);
}

auto
enabled_t::update_permissions(const std::string& prefix, metainfo_t metainfo, std::size_t version, callback_type callback)
    -> std::shared_ptr<api::unicorn_scope_t>
{
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
    }, make_path(prefix), value_t(std::move(metainfo)), version);
}

} // namespace unicorn
} // namespace authorization
} // namespace cocaine
