#pragma once

#include <cocaine/common.hpp>
#include <cocaine/locked_ptr.hpp>
#include <cocaine/repository.hpp>

#include "cocaine/api/auth.hpp"

namespace cocaine {
namespace api {

template<>
struct category_traits<auth_t> {
    typedef std::shared_ptr<auth_t> ptr_type;

    struct factory_type : public basic_factory<auth_t> {
        virtual
        ptr_type
        get(context_t& context, const std::string& name, const std::string& service, const dynamic_t& args) = 0;
    };

    template<class T>
    struct default_factory : public factory_type {
        ptr_type
        get(context_t& context, const std::string& name, const std::string& service, const dynamic_t& args) override {
            ptr_type instance;

            instances.apply([&](std::map<std::string, std::weak_ptr<auth_t>>& instances) {
                auto weak_ptr = instances[name];

                if ((instance = weak_ptr.lock()) == nullptr) {
                    instance = std::make_shared<T>(context, name, service, args);
                    instances[name] = instance;
                }
            });

            return instance;
        }

    private:
        synchronized<std::map<std::string, std::weak_ptr<auth_t>>> instances;
    };
};

}  // namespace api
}  // namespace cocaine
