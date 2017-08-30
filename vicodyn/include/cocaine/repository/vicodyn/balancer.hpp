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

#include "cocaine/api/vicodyn/balancer.hpp"

#include <cocaine/common.hpp>
#include <cocaine/locked_ptr.hpp>
#include <cocaine/repository.hpp>

namespace cocaine {
namespace api {

template<>
struct category_traits<vicodyn::balancer_t> {
    typedef vicodyn::balancer_ptr ptr_type;

    struct factory_type : public basic_factory<vicodyn::balancer_t> {
        virtual
        auto get(context_t& context, cocaine::vicodyn::peers_t& peers, asio::io_service& io_context,
                 const std::string& name, const dynamic_t& args) -> ptr_type = 0;
    };

    template<class T>
    struct default_factory : public factory_type {
        auto get(context_t& context, cocaine::vicodyn::peers_t& peers, asio::io_service& io_context,
                 const std::string& name, const dynamic_t& args) -> ptr_type override
        {
            return ptr_type(new T(context, peers, io_context, name, args));
        }
    };
};

} // namespace api
} // namespace cocaine
