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
#include <cocaine/asio/reactor.hpp>
#include <cocaine/dispatch.hpp>

namespace cocaine { namespace io {

struct cache_tag;

namespace cache {
    struct put {
        typedef cache_tag tag;

        static const char* alias() {
            return "put";
        }

        typedef boost::mpl::list<
            /* key */ std::string,
            /* value */ std::string
        > tuple_type;

        typedef void result_type;
    };

    struct get {
        typedef cache_tag tag;

        static const char* alias() {
            return "get";
        }

        typedef boost::mpl::list<
            /* key */ std::string
        > tuple_type;

        typedef boost::mpl::list<
            /* exists */ bool,
            /* value */ std::string
        > result_type;
    };
}

template<>
struct protocol<cache_tag> {
    typedef mpl::list<
        cache::get,
        cache::put
    > type;

    typedef boost::mpl::int_<
        1
    >::type version;
};

} // namespace io

namespace service {

class cache_t:
    public api::service_t,
    public implements<io::cache_tag>
{
    public:
        typedef tuple::fold<io::cache::get::result_type>::type get_tuple;

        cache_t(context_t& context,
                io::reactor_t& reactor,
                const std::string& name,
                const Json::Value& args);

        virtual
        dispatch_t&
        prototype() {
            return *this;
        }

    private:
        void
        put(const std::string& key,
            const std::string& value);

        get_tuple
        get(const std::string& key);

    private:
        std::shared_ptr<logging::log_t> log_;
        cache::lru_cache<std::string, std::string> cache_;
};

}} // namespace cocaine::service

#endif
