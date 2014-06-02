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

#include "lru_cache.hpp"

#include <cocaine/api/service.hpp>

#include "cocaine/idl/cache.hpp"

#include <cocaine/rpc/dispatch.hpp>

namespace cocaine { namespace service {

class cache_t:
    public api::service_t,
    public dispatch<io::cache_tag>
{
    public:
        typedef result_of<io::cache::get>::type get_result_type;

        cache_t(context_t& context, io::reactor_t& reactor, const std::string& name, const dynamic_t& args);

        virtual
        auto
        prototype() -> io::basic_dispatch_t& {
            return *this;
        }

    private:
        void
        put(const std::string& key, const std::string& value);

        auto
        get(const std::string& key) -> get_result_type;

    private:
        cache::lru_cache<std::string, std::string> cache_;
};

}} // namespace cocaine::service

#endif
