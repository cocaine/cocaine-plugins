/*
    Copyright (c) 2016+ Anton Matveenko <antmat@me.com>
    Copyright (c) 2016+ Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "cocaine/api/postgres/pool.hpp"

#include <cocaine/common.hpp>
#include <cocaine/locked_ptr.hpp>
#include <cocaine/repository.hpp>

namespace cocaine {
namespace api {

template<>
struct category_traits<api::postgres::pool_t> {
    typedef api::postgres::pool_ptr ptr_type;

    struct factory_type : public basic_factory<api::postgres::pool_t> {
        virtual auto get(context_t& context, const std::string& name, const dynamic_t& args) -> ptr_type = 0;
    };

    template<class T>
    struct default_factory : public factory_type {
        virtual auto get(context_t& context, const std::string& name, const dynamic_t& args) -> ptr_type {
            ptr_type instance;

            instances.apply([&](std::map<std::string, std::weak_ptr<api::postgres::pool_t>>& _instances) {
                auto weak_ptr = _instances[name];

                if ((instance = weak_ptr.lock()) == nullptr) {
                    instance = std::make_shared<T>(context, name, args);
                    _instances[name] = instance;
                }
            });

            return instance;
        }

    private:
        synchronized<std::map<std::string, std::weak_ptr<api::postgres::pool_t>>> instances;
    };
};

}
} // namespace cocaine::api

