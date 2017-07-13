//
// All prefix correctness checks are done by unicorn backend,
// unicat is just a dummy proxy!
//
#include <cocaine/format.hpp>
#include <cocaine/errors.hpp>

#include <boost/assert.hpp>

#include <blackhole/logger.hpp>
#include <blackhole/scope/holder.hpp>
#include <blackhole/wrapper.hpp>

#include <cocaine/logging.hpp>

#include "cocaine/idl/unicorn.hpp"

#include "unicorn.hpp"

namespace cocaine { namespace unicat {

namespace detail {
    // TODO: make global in core?
    const auto ACL_NODE = std::string{"/.acls"};

    auto make_dynamic_from_meta(const auth::metainfo_t& metainfo) -> dynamic_t {
        return std::make_tuple(
            std::move(metainfo.c_perms),
            std::move(metainfo.u_perms));
    }

    auto make_acl_path(const std::string& path) -> std::string {
        // Note that path was validated in authorization::enabled_t::verify
        return cocaine::format("{}{}", ACL_NODE, path);
    }
}

unicorn_backend_t::unicorn_backend_t(const options_t& options) :
    backend_t(options),
    access(options.access_handlers_cache->make_handler<api::authorization::unicorn_t>(options.name)),
    backend(api::unicorn(options.ctx_ref, options.name))
{
    COCAINE_LOG_DEBUG(this->logger(), "unicat::unicorn backend started '{}'", this->get_options().name);
}

unicorn_backend_t::~unicorn_backend_t()
{
    COCAINE_LOG_DEBUG(this->logger(), "unicat::unicorn backend detached '{}'", this->get_options().name);
}

auto
unicorn_backend_t::async_verify_read(const std::string& entity, async::verify_handler_t hnd) -> void
{
    return async::verify<io::unicorn::get>(*access, hnd, entity, *hnd.identity);
}

auto
unicorn_backend_t::async_verify_write(const std::string& entity, async::verify_handler_t hnd) -> void
{
    return async::verify<io::unicorn::put>(*access, hnd, entity, *hnd.identity);
}

auto
unicorn_backend_t::async_read_metainfo(const std::string& entity, std::shared_ptr<async::read_handler_t> hnd) -> void
{
    COCAINE_LOG_DEBUG(this->logger(), "unicat::unicorn read metainfo for {}", detail::make_acl_path(entity));
    auto scope = backend->get(
        [=] (std::future<unicorn::versioned_value_t> fut) {
            hnd->on_read(std::move(fut));
        },
        detail::make_acl_path(entity));

    hnd->attach_scope(std::move(scope));
}

auto
unicorn_backend_t::async_write_metainfo(const std::string& entity, const version_t version, const auth::metainfo_t& meta, std::shared_ptr<async::write_handler_t> hnd) -> void
{
    COCAINE_LOG_DEBUG(this->logger(), "unicat::unicorn writing metainfo for {}", detail::make_acl_path(entity));

    using namespace auth;
    auto scope = backend->put(
        [=] (std::future<api::unicorn_t::response::put> fut) {
            hnd->on_write(std::move(fut));
        },
        detail::make_acl_path(entity),
        detail::make_dynamic_from_meta(meta),
        version);

    hnd->attach_scope(std::move(scope));
}

}
}
