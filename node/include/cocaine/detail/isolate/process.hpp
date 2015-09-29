/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_PROCESS_ISOLATE_HPP
#define COCAINE_PROCESS_ISOLATE_HPP

#include "cocaine/api/isolate.hpp"

#include <cstdint>

#include <boost/filesystem/path.hpp>

#ifdef COCAINE_ALLOW_CGROUPS
struct cgroup;
#endif

namespace cocaine { namespace isolate {

class process_t:
    public api::isolate_t,
    public std::enable_shared_from_this<process_t>
{
    context_t& m_context;
    asio::io_service& io_context;

    const std::unique_ptr<logging::log_t> m_log;

    const std::string m_name;
    const boost::filesystem::path m_working_directory;
    const uint64_t m_kill_timeout;

#ifdef COCAINE_ALLOW_CGROUPS
    cgroup* m_cgroup;
#endif

public:
    process_t(context_t& context, asio::io_service& io_context, const std::string& name, const dynamic_t& args);

    virtual
   ~process_t();

    virtual
    void
    spool();

    virtual
    void
    async_spawn(const std::string& path, const api::string_map_t& args, const api::string_map_t& environment, api::spawn_handler_t handler) {

        auto self = shared_from_this();

        get_io_service().post([&, self, handler, path, args, environment] {

            //std::unique_ptr<api::handle_t> handle_;
            auto handle_ = self->spawn(path, args, environment);
                
            handler(std::error_code(errno, std::system_category()), handle_);
            
        });

        //async_spawn(const std::string& path, const string_map_t& args, const string_map_t& environment, std::function<void(const std::error_code&)> handler) {
        // auto handle_ = spawn(path, args, environment);
        // get_io_service().post(std::bind(handler, std::error_code(), handle_));
        //get_io_service().post(std::bind(handler, std::error_code()));
    }


    virtual
    std::unique_ptr<api::handle_t>
    spawn(const std::string& path, const api::string_map_t& args, const api::string_map_t& environment);
};

}} // namespace cocaine::isolate

#ifdef COCAINE_ALLOW_CGROUPS
namespace cocaine { namespace error {

auto
cgroup_category() -> const std::error_category&;

}} // namespace cocaine::error
#endif

#endif
