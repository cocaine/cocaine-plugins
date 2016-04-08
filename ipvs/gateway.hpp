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

namespace cocaine { namespace error {
auto
ipvs_category() -> const std::error_category& {
    static ipvs_category_t instance;
    return instance;
}
}}

namespace cocaine { namespace gateway {

class ipvs_config_t
{
public:
    std::string  scheduler;
    unsigned int weight;
};

class ipvs_t:
    public api::gateway_t
{
    class remote_t;

    typedef std::map<partition_t, std::unique_ptr<remote_t>> remote_map_t;

    context_t& m_context;

    const std::unique_ptr<logging::logger_t> m_log;
    const ipvs_config_t m_cfg;

    // Local endpoints to bind virtual services on.
    std::vector<asio::ip::address> m_endpoints;

    // Keeps track of IPVS configuration.
    synchronized<remote_map_t> m_remotes;

public:
    ipvs_t(context_t& context, const std::string& name, const dynamic_t& args);

    virtual
   ~ipvs_t();

    virtual
    auto
    resolve(const partition_t& name) const -> std::vector<asio::ip::tcp::endpoint>;

    virtual
    size_t
    consume(const std::string& uuid,
            const partition_t& name, const std::vector<asio::ip::tcp::endpoint>& endpoints);

    virtual
    size_t
    cleanup(const std::string& uuid, const partition_t& name);
};

}} // namespace cocaine::gateway

#endif
