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

#ifndef _CACHE_HPP_INCLUDED_
#define _CACHE_HPP_INCLUDED_

#include <cocaine/framework/service.hpp>
#include <cocaine/idl/cache.hpp>

namespace cocaine { namespace framework {

class cache_service_t:
    public service_t
{
    public:
        static const unsigned int version = cocaine::io::protocol<cocaine::io::cache_tag>::version::value;

        cache_service_t(std::shared_ptr<service_connection_t> connection) :
            service_t(connection)
        { }

        service_traits<cocaine::io::cache::put>::future_type
        put(const std::string& key, const std::string& value) {
            return call<io::cache::put>(key, value);
        }

        service_traits<cocaine::io::cache::get>::future_type
        get(const std::string& key) {
            return call<io::cache::get>(key);
        }
};

}} // namespace cocaine::framework

#endif /* _CACHE_HPP_INCLUDED_ */
