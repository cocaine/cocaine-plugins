#include "cocaine/api/auth.hpp"

namespace cocaine {
namespace auth {

class anonymous_t : public api::auth_t {
public:
    typedef api::auth_t::callback_type callback_type;

public:
    anonymous_t(context_t& context, const std::string& name, const std::string& service, const dynamic_t& args) {
        (void)context;
        (void)name;
        (void)service;
        (void)args;
    }

    auto token(callback_type callback) -> void override {
        callback({}, {});
    }
};

}   // namespace auth
}   // namespace cocaine
