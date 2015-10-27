/*
* 2015+ Copyright (c) Anton Matveenko <antmat@yandex-team.ru>
* All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*/

#ifndef COCAINE_UNICORN_API_HPP
#define COCAINE_UNICORN_API_HPP

#include "cocaine/unicorn/writable.hpp"

#include <cocaine/repository.hpp>

namespace cocaine { namespace api {

// As Unicorn provides subscription functionality
// this class is dedicated to manage lifetime of such subscriptions, async requests etc
class unicorn_request_scope_t {
public:
    virtual void
    close() = 0;

    virtual
    ~unicorn_request_scope_t(){}
};

typedef std::shared_ptr<unicorn_request_scope_t> unicorn_scope_ptr;

class unicorn_t {
public:
    /**
    * Typedefs for result type. Actual result types are in include/cocaine/idl/unicorn.hpp
    */
    struct response {
        typedef result_of<io::unicorn::put>::type put_result;
        typedef result_of<io::unicorn::create>::type create_result;
        typedef result_of<io::unicorn::del>::type del_result;
        typedef result_of<io::unicorn::increment>::type increment_result;
        typedef result_of<io::unicorn::get>::type get_result;
        typedef result_of<io::unicorn::subscribe>::type subscribe_result;
        typedef result_of<io::unicorn::children_subscribe>::type children_subscribe_result;
        typedef result_of<io::unicorn::lock>::type lock_result;
    };

    unicorn_t(context_t& context, const std::string& name, const dynamic_t& args);

    virtual
    unicorn_scope_ptr
    put(
        unicorn::writable_helper<response::put_result>::ptr result,
        unicorn::path_t path,
        unicorn::value_t value,
        unicorn::version_t version
    ) = 0;

    virtual
    unicorn_scope_ptr
    get(
        unicorn::writable_helper<response::get_result>::ptr result,
        unicorn::path_t path
    ) = 0;

    virtual
    unicorn_scope_ptr
    create(
        unicorn::writable_helper<response::create_result>::ptr result,
        unicorn::path_t path,
        unicorn::value_t value,
        bool ephemeral,
        bool sequence
    ) = 0;

    unicorn_scope_ptr
    create_default(
        unicorn::writable_helper<response::create_result>::ptr result,
        unicorn::path_t path,
        unicorn::value_t value
    ) {
        return create(std::move(result), std::move(path), std::move(value), false, false);
    }

    virtual
    unicorn_scope_ptr
    del(
        unicorn::writable_helper<response::del_result>::ptr result,
        unicorn::path_t path,
        unicorn::version_t version
    ) = 0;

    virtual
    unicorn_scope_ptr
    subscribe(
        unicorn::writable_helper<response::subscribe_result>::ptr result,
        unicorn::path_t path
    ) = 0;

    virtual
    unicorn_scope_ptr
    children_subscribe(
        unicorn::writable_helper<response::children_subscribe_result>::ptr result,
        unicorn::path_t path
    ) = 0;

    virtual
    unicorn_scope_ptr
    increment(
        unicorn::writable_helper<response::increment_result>::ptr result,
        unicorn::path_t path,
        unicorn::value_t value
    ) = 0;

    virtual
    unicorn_scope_ptr
    lock(
        unicorn::writable_helper<response::lock_result>::ptr result,
        unicorn::path_t path
    ) = 0;

    virtual
    ~unicorn_t() {}
};

template<>
struct category_traits<unicorn_t> {
    typedef std::shared_ptr<unicorn_t> ptr_type;

    struct factory_type: public basic_factory<unicorn_t> {
        virtual
        ptr_type
        get(context_t& context, const std::string& name, const dynamic_t& args) = 0;
    };

    template<class T>
    struct default_factory: public factory_type {
        virtual
        ptr_type
        get(context_t& context, const std::string& name, const dynamic_t& args) {
            ptr_type instance;

            instances.apply([&](std::map<std::string, std::weak_ptr<unicorn_t>>& instances_) {
                if((instance = instances_[name].lock()) == nullptr) {
                    instance = std::make_shared<T>(context, name, args);
                    instances_[name] = instance;
                }
            });

            return instance;
        }

    private:
        synchronized<std::map<std::string, std::weak_ptr<unicorn_t>>> instances;
    };
};

category_traits<unicorn_t>::ptr_type
unicorn(context_t& context, const std::string& name);

}}

#endif
