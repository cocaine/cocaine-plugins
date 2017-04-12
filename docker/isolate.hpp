/*
    Copyright (c) 2011-2013 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#ifndef COCAINE_DOCKER_ISOLATE_HPP
#define COCAINE_DOCKER_ISOLATE_HPP

#include "docker_client.hpp"

#include <cocaine/api/isolate.hpp>

#include <mutex>

namespace cocaine { namespace isolate {

class docker_t:
    public api::isolate_t
{
    COCAINE_DECLARE_NONCOPYABLE(docker_t)

public:
    docker_t(context_t& context, asio::io_service& io_context, const std::string& name, const std::string& type, const dynamic_t& args);

    virtual
   ~docker_t();

    virtual
    std::unique_ptr<api::cancellation_t>
    spool(std::shared_ptr<api::spool_handle_base_t> handle);

    virtual
    std::unique_ptr<api::cancellation_t>
    spawn(const std::string& path,
          const api::args_t& args,
          const api::env_t& environment,
          std::shared_ptr<api::spawn_handle_base_t> handle);

    virtual
    void
    metrics(const dynamic_t& query, std::shared_ptr<api::metrics_handle_base_t> handle) const;

private:
    context_t& m_context;
    std::shared_ptr<logging::logger_t> m_log;

    std::mutex spawn_lock;

    std::string m_runtime_path;
    std::string m_image;
    std::string m_tag;
    bool m_do_pool;

    docker::client_t m_docker_client;
    rapidjson::Value m_run_config;
    std::string m_network_mode;
    rapidjson::Value m_additional_capabilities;
    rapidjson::Value::AllocatorType m_json_allocator;
};

}} // namespace cocaine::isolate

#endif // COCAINE_DOCKER_ISOLATE_HPP
