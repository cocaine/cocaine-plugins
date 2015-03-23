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

namespace cocaine { namespace unicorn {
class api_t {
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

    virtual void
    put(
        writable_helper<response::put_result>::ptr result,
        unicorn::path_t path,
        unicorn::value_t value,
        unicorn::version_t version
    ) = 0;

    virtual void
    get(
        writable_helper<response::get_result>::ptr result,
        unicorn::path_t path
    ) = 0;

    virtual void
    create(
        writable_helper<response::create_result>::ptr result,
        unicorn::path_t path,
        unicorn::value_t value,
        bool ephemeral,
        bool sequence
    ) = 0;

    void
    create_default(
        writable_helper<response::create_result>::ptr result,
        unicorn::path_t path,
        unicorn::value_t value
    ) {
        return create(std::move(result), std::move(path), std::move(value), false, false);
    }

    virtual void
    del(
        writable_helper<response::del_result>::ptr result,
        unicorn::path_t path,
        unicorn::version_t version
    ) = 0;

    virtual void
    subscribe(
        writable_helper<response::subscribe_result>::ptr result,
        unicorn::path_t path
    ) = 0;

    virtual void
    children_subscribe(
        writable_helper<response::children_subscribe_result>::ptr result,
        unicorn::path_t path
    ) = 0;

    virtual void
    increment(
        writable_helper<response::increment_result>::ptr result,
        unicorn::path_t path,
        unicorn::value_t value
    ) = 0;

    virtual void
    lock(
        writable_helper<response::lock_result>::ptr result,
        unicorn::path_t path
    ) = 0;

    virtual void
    close() = 0;

    virtual
    ~api_t() {}
};
}}

#endif
