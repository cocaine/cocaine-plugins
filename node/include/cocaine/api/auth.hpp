#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <system_error>

#include <cocaine/forwards.hpp>

namespace cocaine {
namespace api {

class auth_t {
public:
    typedef auth_t category_type;

    struct token_t {
        /// Token type. Think of the first part of the Authorization HTTP header.
        std::string type;

        /// Token body.
        std::string body;

        /// Time point when the token becomes expired. Note that due to NTP misconfiguration,
        /// slow network of whatever else the token may become expired in unexpected way. Use this
        /// information as a cache hint.
        std::chrono::system_clock::time_point expires_in;

        auto is_valid() const -> bool {
            return !is_invalid();
        }

        auto is_invalid() const -> bool {
            return type.empty() || body.empty();
        }

        auto is_expired() const -> bool {
            return std::chrono::system_clock::now() >= expires_in;
        }
    };

    typedef std::function<void(token_t token, const std::error_code& ec)> callback_type;

public:
    virtual ~auth_t() = default;

    /// Tries to obtain, possibly asynchronously, an authorization token with its type and optional
    /// expiration time point.
    ///
    /// \param callback The function which is called on either when the token is read or any error.
    ///     It's strongly recommended to wrap the callback with an executor to avoid deadlocks or
    ///     inner threads context switching.
    virtual auto token(callback_type callback) -> void = 0;
};

auto auth(context_t& context, const std::string& name, const std::string& service) ->
    std::shared_ptr<auth_t>;

}  // namespace api
}  // namespace cocaine
