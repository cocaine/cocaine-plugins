#include <blackhole/logger.hpp>
#include <blackhole/scope/holder.hpp>
#include <blackhole/wrapper.hpp>

#include <cocaine/idl/storage.hpp>
#include <cocaine/logging.hpp>

#include <boost/assert.hpp>

#include "storage.hpp"

namespace cocaine { namespace unicat {

namespace detail {
    // TODO: make global in core?
    const auto ACL_COLLECTION = std::string{".collection-acls"};
    const auto COLLECTION_ACLS_TAGS = std::vector<std::string>{"storage-acls"};
}

storage_backend_t::storage_backend_t(const options_t& options) :
    backend_t(options),
    access(options.access_handlers_cache->make_handler<api::authorization::storage_t>(options.name)),
    backend(api::storage(options.ctx_ref, options.name))
{
    COCAINE_LOG_DEBUG(this->logger(), "unicat::storage backend started '{}'", this->get_options().name);
}

storage_backend_t::~storage_backend_t() {
    COCAINE_LOG_DEBUG(this->logger(), "unicat::storage backend detached '{}'", this->get_options().name);
}

auto
storage_backend_t::async_verify_read(const std::string& entity, async::verify_handler_t hnd) -> void
{
    return async::verify<io::storage::read>(
        *access, hnd, detail::ACL_COLLECTION, entity, *hnd.identity);
}
auto
storage_backend_t::async_verify_write(const std::string& entity, async::verify_handler_t hnd) -> void
{
    return async::verify<io::storage::write>(
        *access, hnd, detail::ACL_COLLECTION, entity, *hnd.identity);
}

auto
storage_backend_t::async_read_metainfo(const std::string& entity, std::shared_ptr<async::read_handler_t> hnd) -> void
{
    return backend->get<auth::metainfo_t>(detail::ACL_COLLECTION, entity,
        [=] (std::future<auth::metainfo_t> fut) {
            hnd->on_read(std::move(fut));
        }
    );
}

auto
storage_backend_t::async_write_metainfo(const std::string& entity, const version_t, const auth::metainfo_t& meta, std::shared_ptr<async::write_handler_t> hnd) -> void
{
    return backend->put(detail::ACL_COLLECTION, entity, meta, detail::COLLECTION_ACLS_TAGS,
        [=] (std::future<void> fut) {
            hnd->on_write(std::move(fut));
        }
    );
}

}
}
