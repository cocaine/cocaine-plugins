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

#include <cocaine/dynamic.hpp>
#include <cocaine/forwards.hpp>

#include <functional>

namespace pqxx {
    class connection_base;
}

namespace cocaine {
namespace api {
namespace postgres {

struct pool_t
{
    typedef pool_t category_type;

    virtual ~pool_t() {
        // Empty.
    }

    virtual void
    execute(std::function<void(pqxx::connection_base&)> function) = 0;

protected:
    pool_t(context_t& /* context */,
           const std::string& /* name */,
           const dynamic_t& /* args */) { }

};

typedef std::shared_ptr<pool_t> pool_ptr;

pool_ptr pool(context_t& context, const std::string& name);

} // namespace postgres
} // namesapce api
} // namespace cocaine

