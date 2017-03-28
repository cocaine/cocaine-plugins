/*
* 2013+ Copyright (c) Alexander Ponomarev <noname@yandex-team.ru>
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

#ifndef COCAINE_CACHE_SERVICE_HPP
#define COCAINE_CACHE_SERVICE_HPP

#include <cocaine/api/service.hpp>

#include "cocaine/idl/cache.hpp"
#include <cocaine/rpc/dispatch.hpp>

#include <cocaine/locked_ptr.hpp>

#include "lru_cache.hpp"

namespace cocaine { namespace service {

class cache_t:
    public api::service_t,
    public dispatch<io::cache_tag>
{
    public:
        cache_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args);

        virtual
        auto
        prototype() -> io::basic_dispatch_t& {
            return *this;
        }

    private:
        void
        put(const std::string& key, const std::string& value);

        auto
        get(const std::string& key) -> result_of<io::cache::get>::type;

    private:
        synchronized<cache::lru_cache<std::string, std::string>> cache_;
};

}} // namespace cocaine::service

#endif
