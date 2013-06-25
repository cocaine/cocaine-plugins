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

#include "gateway.hpp"

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

using namespace cocaine::io;
using namespace cocaine::gateway;

ipvs_t::ipvs_t(context_t& context,
               const std::string& name,
               const Json::Value& args):
    category_type(context, reactor, name, args),
    m_log(new logging::log_t(context, name))
{
    if(::ipvs_init() != 0) {
        throw configuration_error_t(
            "unable to initialize the IPVS library - [%d] %s",
            errno,
            ::ipvs_strerror(errno)
        );
    }

    COCAINE_LOG_INFO(m_log, "using IPVS version %d", ::ipvs_version());
}

ipvs_t::~ipvs_t() {
    ::ipvs_close();
}

resolve_result_type
ipvs_t::resolve(const std::string& name) const {
    auto it = m_remote_services.find(name);

    if(it == m_remote_services.end()) {
        throw cocaine::error_t("the specified service is not available in the group");
    }

    auto endpoint = std::get<0>(it->second.info);

    COCAINE_LOG_DEBUG(
        m_log,
        "providing '%s' using virtual service on %s:%d",
        name,
        std::get<0>(endpoint),
        std::get<1>(endpoint)
    );

    return it->second.info;
}

void
ipvs_t::consume(const std::string& uuid, synchronize_result_type dump) {
    COCAINE_LOG_DEBUG(m_log, "updating services for node '%s'", uuid);

    remove_service_map_t update;

    for(auto it = dump.begin(); it != dump.end(); ++it) {
        ipvs_service_t service;

        // Setup the virtual service

        std::memset(&service, 0, sizeof(service));

        io::tcp::endpoint bindpoint("0.0.0.0", 16000);

        std::memcpy(
            &service.addr.in,
            &static_cast<sockaddr_in*>(bindpoint.data())->sin_addr,
            sizeof(in_addr_t)
        );

        service.af         = bindpoint.family();
        service.port       = bindpoint.port();
        service.protocol   = IPPROTO_TCP;
        service.sched_name = "wlc";

        ipvs_dest_t target;

        // Setup the backend destination

        std::memset(&target, 0, sizeof(ipvs_dest_t));

        std::string hostname;
        uint16_t    port;

        std::tie(hostname, port) = std::get<0>(it->second);

        io::tcp::endpoint endpoint = io::tcp::resolver::query(hostname, port);

        std::memcpy(
            &target.addr.in,
            &static_cast<sockaddr_in*>(endpoint.data())->sin_addr,
            sizeof(in_addr_t)
        );

        target.af         = target.family();
        target.port       = target.port();
        target.conn_flags = IP_VS_CONN_F_MASQ;
        target.weight     = 1;

        // Create the resolver entry

        resolve_result_type info = {
            std::make_tuple(m_context.config.network.hostname, port),
            std::get<1>(it->second),
            std::get<2>(it->second)
        };

        update[it->first] = {
            service,
            {{ uuid, target }},
            info
        };
    }

    merge(update);
}

void
ipvs_t::prune(const std::string& uuid) {
    remove_service_map_t update(m_remote_services);

    COCAINE_LOG_DEBUG(m_log, "pruning services for node '%s'", uuid);

    auto it = update.begin(), end = update.end();

    while(it != end) {
        auto& service = it->second;
        auto  backend = service.backends.find(uuid);

        if(backend != service.backends.end()) {
            service.backends.erase(backend);
    
            if(service.backends.empty()) {
                update.erase(it++);
                continue;
            }
        }

        ++it;
    }

    merge(update);
}
