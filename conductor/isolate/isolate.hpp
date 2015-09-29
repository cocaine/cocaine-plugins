/*
    Copyright (c) 2011-2013 Dmitry Unkovsky <diunko@yandex-team.ru>
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

#ifndef COCAINE_RPC_ISOLATE_HPP
#define COCAINE_RPC_ISOLATE_HPP

#include <cocaine/api/isolate.hpp>
#include <cocaine/context.hpp>

#include "cocaine/conductor.hpp"

namespace cocaine { namespace isolate {

class rpc_isolate_t:
    public api::isolate_t,
    public std::enable_shared_from_this<rpc_isolate_t>
{
    COCAINE_DECLARE_NONCOPYABLE(rpc_isolate_t)

public:
    rpc_isolate_t(context_t& context, asio::io_service& io_context, const std::string& name, const dynamic_t& args);

    virtual
   ~rpc_isolate_t();

    virtual
    void
    spool(){
        // Empty
        COCAINE_LOG_DEBUG(m_log, "================ fake spool ================");
    }

    virtual
    std::unique_ptr<api::cancellation_t>
    async_spool(std::function<void(const std::error_code&)> handler);

    virtual
    std::unique_ptr<api::handle_t>
    spawn(const std::string& path, const api::string_map_t& args, const api::string_map_t& environment);
    // {
    //     return std::unique_ptr<api::handle_t>();
    // }

    virtual
    void
    async_spawn(const std::string& path,
                const api::string_map_t& args,
                const api::string_map_t& environment,
                api::spawn_handler_t handler);
    
    virtual
    void
    terminate(uint64_t container_id) {
        COCAINE_LOG_DEBUG(m_log, "terminating container %d", container_id);
        
        // Empty for now
    }

    std::shared_ptr<cocaine::logging::log_t> m_log;

private:

    std::string m_runtime_path;
    std::string m_image;
    std::string m_tag;
    bool m_do_pool;


    context_t& m_context;
    const dynamic_t m_args;

};

}} // namespace cocaine::isolate

#endif // COCAINE_RPC_ISOLATE_HPP
