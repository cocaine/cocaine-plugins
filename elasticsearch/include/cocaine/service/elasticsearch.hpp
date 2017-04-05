/*
    Copyright (c) 2011-2013 Evgeny Safronov <esafronov@yandex-team.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#include <boost/asio/io_service.hpp>

#include <cocaine/api/service.hpp>

#include "cocaine/idl/elasticsearch.hpp"

#include <cocaine/rpc/dispatch.hpp>

#include <swarm/urlfetcher/url_fetcher.hpp>

namespace cocaine { namespace service {

namespace response {
typedef result_of<io::elasticsearch::get>::type get;
typedef result_of<io::elasticsearch::index>::type index;
typedef result_of<io::elasticsearch::search>::type search;
typedef result_of<io::elasticsearch::delete_index>::type delete_index;
}

class elasticsearch_t :
    public api::service_t,
    public dispatch<io::elasticsearch_tag>
{
    class impl_t;
    std::unique_ptr<impl_t> d;

public:
    elasticsearch_t(context_t& context,
                    asio::io_service& asio,
                    const std::string& name,
                    const dynamic_t& args);

   ~elasticsearch_t();

    auto
    prototype() -> io::basic_dispatch_t& {
        return *this;
    }

    deferred<response::get>
    get(const std::string& index,
        const std::string& type,
        const std::string& id) const;

    deferred<response::index>
    index(const std::string& data,
          const std::string& index,
          const std::string& type,
          const std::string& id) const;

    deferred<response::search>
    search(const std::string& index,
           const std::string& type,
           const std::string& query,
           int size = io::elasticsearch::search::DEFAULT_SIZE) const;

    deferred<response::delete_index>
    delete_index(const std::string& index,
                 const std::string& type,
                 const std::string& id) const;
};

} }
