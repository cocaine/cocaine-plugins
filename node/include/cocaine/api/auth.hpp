#pragma once

#include <functional>
#include <string>
#include <system_error>

namespace cocaine {
inline namespace v1 {
namespace api {

class auth_t {
public:
    struct token_t {
        /// Token type. Think of the first part of the Authorization HTTP header.
        std::string type;

        /// Token body.
        std::string body;

        /// Time point when the token becomes expired. Note that due to NTP misconfiguration,
        /// slow network of whatever else the token may become expired in unexpected way. Use this
        /// information as a cache hint.
        std::chrono::time_point expires_in;
    };

public:
    virtual ~auth_t() = default;

    /// Tries to obtain, possibly asynchronously, an authorization token with its type and optional
    /// expiration time point.
    ///
    /// \param callback The function which is called on either when the token is read or any error.
    ///     It's strongly recommended to wrap the callback with an executor to avoid deadlocks or
    ///     inner threads context switching.
    virtual auto token(std::function<void(token_t token, const std::error_code& ec)> callback) = 0;
};

}  // namespace api
}  // namespace v1
}  // namespace cocaine
