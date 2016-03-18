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

#include <system_error>
#include <string>

#include <cocaine/forwards.hpp>

namespace cocaine { namespace isolate {

void* init_cgroups(const char* cgroup_name, const dynamic_t& args, logging::logger_t& log);

void destroy_cgroups(void* cgroup_ptr, logging::logger_t& log);

void attach_cgroups(void* cgroup_ptr, logging::logger_t& log);

const char* get_cgroup_error(int code);

} // namespace isolate
} // namespace cocaine

namespace cocaine {
namespace error {

struct cgroup_category_t:
public std::error_category
{
    virtual
    auto
    name() const throw() -> const char* {
        return "cocaine.isolate.process.cgroups";
    }

    virtual
    auto
    message(int code) const -> std::string {
        return isolate::get_cgroup_error(code);
    }
};

auto cgroup_category() -> const std::error_category&;

} // namespace error
} // namespace cocaine

