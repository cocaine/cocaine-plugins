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

#include <cocaine/asio/tcp.hpp>

#include <queue>

extern "C" {
    #include "libipvs-1.25/libipvs.h"
}

namespace cocaine { namespace gateway {

class ipvs_t:
    public api::gateway_t
{
    public:
        ipvs_t(context_t& context, const std::string& name, const Json::Value& args);

        virtual
       ~ipvs_t();

        virtual
        api::resolve_result_type
        resolve(const std::string& name) const;

        virtual
        void
        consume(const std::string& uuid, const api::synchronize_result_type& dump);

        virtual
        void
        cleanup(const std::string& uuid);

    private:
        struct service_info_t {
            unsigned int version;

            // NOTE: There's only one service info for all the services in the cluster, which
            // means that all the services should expose the same protocol, otherwise bad things
            // gonna happen.
            std::tuple_element<2, api::resolve_result_type>::type map;
        };

        void
        add_service(const std::string& name, const service_info_t& info);

        void
        add_backend(const std::string& name, const std::string& uuid, ipvs_dest_t backend);

        void
        pop_service(const std::string& name);

        void
        pop_backend(const std::string& name, const std::string& uuid);

    private:
        context_t& m_context;
        std::unique_ptr<logging::log_t> m_log;

        const std::string m_default_scheduler;
        const unsigned    m_default_weight;

        // Ports available for allocation to virtual services.
        std::priority_queue<uint16_t, std::vector<uint16_t>, std::greater<uint16_t>> m_ports;

        // Keeps track of service versions and mappings.
        std::map<std::string, service_info_t> m_service_info;

        struct remote_service_t {
            ipvs_service_t handle;

            // Service endpoint.
            io::tcp::endpoint endpoint;

            // Precooked endpoint tuple.
            io::locator::endpoint_tuple_type cooked;

            // Backend UUID -> Destination mapping.
            std::map<std::string, ipvs_dest_t> backends;
        };

        // Keeps track of IPVS configuration.
        std::map<std::string, remote_service_t> m_remote_services;

        // Keeps track of last update from every node to effectively drop stale backends.
        std::map<std::string, api::synchronize_result_type> m_history;
};

}} // namespace cocaine::gateway

#endif
