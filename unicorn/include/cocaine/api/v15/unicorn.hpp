#pragma once

#include <cocaine/api/unicorn.hpp>
#include <cocaine/errors.hpp>

namespace cocaine {
namespace api {
namespace v15 {

class unicorn_t: public api::unicorn_t {
public:
    unicorn_t(context_t& context, const std::string& name, const dynamic_t& args):
        api::unicorn_t(context, name, args){}

    virtual
    unicorn_scope_ptr
    named_lock(callback::lock callback, const unicorn::path_t& path, const unicorn::value_t& value) = 0;
};

typedef std::shared_ptr<unicorn_t> unicorn_ptr;

inline
unicorn_ptr
unicorn(context_t& context, const std::string& name) {

    auto unicorn = api::unicorn(context, name);
    auto v15_ptr = std::dynamic_pointer_cast<v15::unicorn_t>(unicorn);
    if(!v15_ptr) {
        throw error_t(error::component_not_found, "unicorn component \"{}\" do not implement v15 interface", name);
    }
    return v15_ptr;
}

} // namespace v15
} // namespace api
} // namespace cocaine
