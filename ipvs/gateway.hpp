/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_IPVS_GATEWAY_HPP
#define COCAINE_IPVS_GATEWAY_HPP

#include <cocaine/api/gateway.hpp>
#include <cocaine/locked_ptr.hpp>

namespace cocaine { namespace error {
auto
ipvs_category() -> const std::error_category&;
}}

namespace cocaine { namespace gateway {

class ipvs_config_t
{
public:
    std::string  scheduler;
    unsigned int weight;
    std::string x_cocaine_cluster;
};

class ipvs_t:
    public api::gateway_t
{
    class remote_t;

    typedef std::map<std::string, std::unique_ptr<remote_t>> remote_map_t;

    context_t& m_context;

    const std::unique_ptr<logging::logger_t> m_log;
    const ipvs_config_t m_cfg;

    // Local endpoints to bind virtual services on.
    std::vector<asio::ip::address> m_endpoints;

    // Keeps track of IPVS configuration.
    synchronized<remote_map_t> m_remotes;

    std::string local_uuid;
public:
    ipvs_t(context_t& context, const std::string& local_uuid, const std::string& name, const dynamic_t& args,
           const dynamic_t::object_t& extra);

    virtual
   ~ipvs_t();

    auto
    resolve_policy() const -> resolve_policy_t {
        return resolve_policy_t::remote_only;
    }

    auto
    resolve(const std::string& name) const -> service_description_t;

    auto
    consume(const std::string& uuid,
            const std::string& name,
            unsigned int version,
            const std::vector<asio::ip::tcp::endpoint>& endpoints,
            const io::graph_root_t& protocol,
            const dynamic_t::object_t& extra) -> void override;

    auto
    cleanup(const std::string& uuid, const std::string& name) -> void override;

    auto
    cleanup(const std::string& uuid) -> void override;

    auto
    total_count(const std::string& name) const -> size_t override;
};

}} // namespace cocaine::gateway

#endif
