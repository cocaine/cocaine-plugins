#pragma once

#include <functional>
#include <future>
#include <memory>
#include <tuple>
#include <utility>

#include <system_error>

#include <boost/thread.hpp>
#include <boost/thread/scoped_thread.hpp>

#include <cocaine/context.hpp>
#include <cocaine/auth/uid.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/rpc/session.hpp>

#include <cocaine/api/unicorn.hpp>
#include <cocaine/unicorn/value.hpp>

#include "cocaine/service/unicat/auth/metainfo.hpp"

#include "auth_cache.hpp"

namespace cocaine { namespace unicat {

using version_t = unicorn::version_t;

namespace async {

// Used in unicorn backend reincarnation for scope management.
struct scope_ctl_t {
    auto attach_scope(api::unicorn_scope_ptr s) -> void {
        if (scope) {
            scope->close();
        }
        scope = s;
    }

    virtual ~scope_ctl_t() {
        if (scope) {
            scope->close();
        }
    }

    api::unicorn_scope_ptr scope;
};

struct read_handler_t : scope_ctl_t {
    virtual auto on_read(std::future<unicorn::versioned_value_t>) -> void = 0;
    virtual auto on_read(std::future<auth::metainfo_t>) -> void = 0;

    virtual ~read_handler_t() = default;
};

struct write_handler_t : scope_ctl_t {
    virtual auto on_write(std::future<void>) -> void = 0;
    virtual auto on_write(std::future<api::unicorn_t::response::put>) -> void = 0;

    virtual ~write_handler_t() = default;
};

struct verify_handler_t {
    const std::shared_ptr<const auth::identity_t> identity;
    std::function<void(std::error_code)> func;

    auto operator()(std::error_code ec) -> void {
        return func(ec);
    }
};

template<typename Event, typename Access, typename... Args>
auto
verify(Access&& access, verify_handler_t hnd, Args&&... args) -> void {
    return access.template verify<Event>(std::forward<Args>(args)..., hnd);
}
} // async

class backend_t {
public:
    struct options_t {
        context_t& ctx_ref;
        std::string name; // service name
        std::shared_ptr<logging::logger_t> log;
        std::shared_ptr<authorization::handlers_cache_t> access_handlers_cache;
    };

    explicit backend_t(const options_t& options);
    virtual ~backend_t() {}

    auto logger() -> std::shared_ptr<logging::logger_t>;
    auto get_options() -> options_t;

    // Verify handlers should store identity
    virtual auto async_verify_read(const std::string& entity, async::verify_handler_t) -> void = 0;
    virtual auto async_verify_write(const std::string& entity, async::verify_handler_t) -> void = 0;

    virtual auto async_read_metainfo(const std::string& entity, std::shared_ptr<async::read_handler_t>) -> void = 0;
    virtual auto async_write_metainfo(const std::string& entity, const version_t, const auth::metainfo_t& meta, std::shared_ptr<async::write_handler_t>) -> void = 0;
private:
    options_t options;
};

}
}
