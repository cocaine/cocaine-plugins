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

#include <cocaine/asio/resolver.hpp>
#include <cocaine/asio/tcp.hpp>

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

#include <cstring>
#include <system_error>

#include <netinet/in.h>

using namespace cocaine::api;
using namespace cocaine::io;
using namespace cocaine::gateway;

namespace {

class ipvs_category_t:
    public std::error_category
{
    virtual
    const char*
    name() const throw() {
        return "ipvs";
    }

    virtual
    std::string
    message(int code) const {
        return ::ipvs_strerror(code);
    }
};

ipvs_category_t category_instance;

const std::error_category&
ipvs_category() {
    return category_instance;
}

}

ipvs_t::ipvs_t(context_t& context, const std::string& name, const Json::Value& args):
    category_type(context, name, args),
    m_context(context),
    m_log(new logging::log_t(context, name)),
    m_default_scheduler(args.get("scheduler", "wlc").asString()),
    m_default_weight(args.get("weight", 1).asUInt())
{
    if(::ipvs_init() != 0) {
        throw std::system_error(errno, ipvs_category(), "unable to initialize IPVS");
    }

    COCAINE_LOG_INFO(m_log, "using IPVS version %d", ::ipvs_version());

    if(args["port-range"].empty()) {
        throw cocaine::error_t("no port ranges have been specified");
    }

    uint16_t min = args["port-range"][0].asUInt(),
             max = args["port-range"][1].asUInt();

    COCAINE_LOG_INFO(m_log, "%u gateway ports available, %u through %u", max - min, min, max);

    while(min != max) {
        m_ports.push(--max);
    }

    ::ipvs_flush();
}

ipvs_t::~ipvs_t() {
    COCAINE_LOG_INFO(m_log, "cleaning up the virtual services");

    for(auto it = m_remote_services.begin(); it != m_remote_services.end(); ++it) {
        ::ipvs_del_service(&it->second.handle);
    }

    ::ipvs_close();
}

resolve_result_type
ipvs_t::resolve(const std::string& name) const {
    auto ipvs = m_remote_services.find(name);

    if(ipvs == m_remote_services.end()) {
        throw cocaine::error_t("the specified service is not available in the group");
    }

    COCAINE_LOG_DEBUG(
        m_log,
        "providing '%s' using virtual service on %s:%d",
        name,
        std::get<0>(ipvs->second.endpoint),
        std::get<1>(ipvs->second.endpoint)
    );

    auto info = m_service_info.find(name);

    BOOST_VERIFY(info != m_service_info.end());

    return std::make_tuple(
        ipvs->second.endpoint,
        info->second.version,
        info->second.map
    );
}

void
ipvs_t::consume(const std::string& uuid, synchronize_result_type dump) {
    COCAINE_LOG_DEBUG(m_log, "updating node '%s'", uuid);

    for(auto it = dump.begin(); it != dump.end(); ++it) {
        ipvs_dest_t backend;

        // Store the service info

        if(m_service_info.find(it->first) == m_service_info.end()) {
            service_info_t info = {
                std::get<1>(it->second),
                std::get<2>(it->second)
            };

            m_service_info[it->first] = info;
        }

        // Setup the backend destination

        std::memset(&backend, 0, sizeof(ipvs_dest_t));

        std::string hostname;
        uint16_t    port;

        std::tie(hostname, port) = std::get<0>(it->second);

        io::tcp::endpoint endpoint = io::resolver<io::tcp>::query(hostname, port);

        std::memcpy(
            &backend.addr.in,
            &reinterpret_cast<sockaddr_in*>(endpoint.data())->sin_addr,
            sizeof(in_addr_t)
        );

        backend.af         = AF_INET;
        backend.conn_flags = IP_VS_CONN_F_MASQ;
        backend.port       = htons(endpoint.port());
        backend.weight     = m_default_weight;

        add_backend(it->first, uuid, backend);
    }

    if(m_history.find(uuid) != m_history.end()) {
        std::vector<std::pair<std::string, api::resolve_result_type>> stale;

        std::set_difference(
            m_history[uuid].begin(),
            m_history[uuid].end(),
            dump.begin(),
            dump.end(),
            std::back_inserter(stale),
            dump.value_comp()
        );

        for(auto it = stale.begin(); it != stale.end(); ++it) {
            pop_backend(it->first, uuid);
        }
    }

    m_history[uuid] = dump;
}

void
ipvs_t::prune(const std::string& uuid) {
    COCAINE_LOG_DEBUG(m_log, "pruning node '%s'", uuid);

    std::vector<std::string> names;

    std::transform(
        m_remote_services.begin(),
        m_remote_services.end(),
        std::back_inserter(names),
        tuple::nth_element<0>()
    );

    for(auto it = names.begin(); it != names.end(); ++it) {
        pop_backend(*it, uuid);
    }

    m_history.erase(uuid);
}

void
ipvs_t::add_backend(const std::string& name, const std::string& uuid, ipvs_dest_t backend) {
    auto it = m_remote_services.find(name);

    if(it == m_remote_services.end()) {
        ipvs_service_t service;

        // Setup the virtual service

        std::memset(&service, 0, sizeof(service));

        io::tcp::endpoint endpoint = io::resolver<io::tcp>::query(
            m_context.config.network.hostname,
            m_ports.top()
        );

        std::memcpy(
            &service.addr.in,
            &reinterpret_cast<sockaddr_in*>(endpoint.data())->sin_addr,
            sizeof(in_addr_t)
        );

        service.af         = AF_INET;
        service.port       = htons(endpoint.port());
        service.protocol   = IPPROTO_TCP;

        std::strncpy(service.sched_name, m_default_scheduler.c_str(), IP_VS_SCHEDNAME_MAXLEN);

        COCAINE_LOG_INFO(m_log, "adding virtual service '%s' on port %d", name, endpoint.port());

        if(::ipvs_add_service(&service) != 0) {
            COCAINE_LOG_ERROR(m_log, "unable to add a virtual service - [%d] %s", errno, ::ipvs_strerror(errno));
            return;
        }

        m_ports.pop();

        remote_service_t remote = { service, std::make_tuple(
            endpoint.address(),
            endpoint.port()
        )};

        std::tie(it, std::ignore) = m_remote_services.insert(std::make_pair(
            name,
            remote
        ));
    }

    if(it->second.backends.find(uuid) != it->second.backends.end()) {
        return;
    }

    COCAINE_LOG_INFO(m_log, "adding node '%s' to virtual service '%s'", uuid, name);

    if(::ipvs_add_dest(&it->second.handle, &backend) != 0) {
        COCAINE_LOG_ERROR(m_log, "unable to add a node - [%d] %s", errno, ::ipvs_strerror(errno));
        return;
    }

    it->second.backends[uuid] = backend;
}

void
ipvs_t::pop_backend(const std::string& name, const std::string& uuid) {
    auto it = m_remote_services.find(name);

    if(it == m_remote_services.end()) {
        return;
    }

    auto& service = it->second;
    auto  backend = service.backends.find(uuid);

    if(backend != service.backends.end()) {
        COCAINE_LOG_INFO(m_log, "removing node '%s' from virtual service '%s'", uuid, name);

        if(::ipvs_del_dest(&service.handle, &backend->second) != 0) {
            COCAINE_LOG_ERROR(m_log, "unable to remove a node - [%d] %s", errno, ::ipvs_strerror(errno));
            return;
        }

        service.backends.erase(uuid);
    }

    if(service.backends.empty()) {
        COCAINE_LOG_INFO(m_log, "removing virtual service '%s'", name);

        if(::ipvs_del_service(&service.handle) != 0) {
            COCAINE_LOG_ERROR(m_log, "unable to remove a virtual service - [%d] %s", errno, ::ipvs_strerror(errno));
            return;
        }

        m_ports.push(ntohs(service.handle.port));

        // Drop the IPVS state.
        m_remote_services.erase(it);

        // Drop the service info, so that it won't lay around stale.
        m_service_info.erase(name);
    }
}
