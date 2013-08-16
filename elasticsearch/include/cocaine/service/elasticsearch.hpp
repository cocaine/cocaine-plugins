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

#include <cocaine/api/service.hpp>
#include <cocaine/asio/reactor.hpp>

#include <swarm/networkrequest.h>
#include <swarm/networkmanager.h>

#include "cocaine/io/protocol.hpp"

namespace cocaine { namespace service {

namespace response {
typedef io::event_traits<io::elasticsearch::get>::result_type get;
typedef io::event_traits<io::elasticsearch::index>::result_type index;
typedef io::event_traits<io::elasticsearch::search>::result_type search;
typedef io::event_traits<io::elasticsearch::delete_index>::result_type delete_index;
}

class elasticsearch_t : public api::service_t {
    class impl_t;
    std::unique_ptr<impl_t> d;

public:
    elasticsearch_t(context_t &context, io::reactor_t &reactor, const std::string& name, const Json::Value& args);
    ~elasticsearch_t();

    cocaine::deferred<response::get>
    get(const std::string &index, const std::string &type, const std::string &id) const;

    cocaine::deferred<response::index>
    index(const std::string &data, const std::string &index, const std::string &type, const std::string &id) const;

    cocaine::deferred<response::search>
    search(const std::string &index, const std::string &type, const std::string &query, int size = 10) const;

    cocaine::deferred<response::delete_index>
    delete_index(const std::string &index, const std::string &type, const std::string &id) const;
};

} }
