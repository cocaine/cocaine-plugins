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
    COCAINE_LOG_INFO(m_log, "cleaning up");

    for(auto it_ip4 = m_remote_services_ip4.begin(); it_ip4 != m_remote_services_ip4.end(); ++it_ip4) {
        ::ipvs_del_service(&it_ip4->second.handle);
    }

    for(auto it_ip6 = m_remote_services_ip6.begin(); it_ip6 != m_remote_services_ip6.end(); ++it_ip6) {
        ::ipvs_del_service(&it_ip6->second.handle);
    }

    ::ipvs_close();
}

ipvs_t::metadata_t
ipvs_t::resolve(const std::string& name) const {
    auto ipvs = m_remote_services_ip4.find(name);

    if(ipvs == m_remote_services_ip4.end()) {
        ipvs = m_remote_services_ip6.find(name);
        if(ipvs == m_remote_services_ip6.end()) {

            throw cocaine::error_t("the specified service is not available");

        }
    }

    const auto info = m_service_info.find(name);

    BOOST_VERIFY(info != m_service_info.end());

    COCAINE_LOG_DEBUG(m_log, "providing '%s' using virtual service on '%s':'%d'", name, std::get<0>(ipvs->second.cooked), std::get<1>(ipvs->second.cooked));

    return metadata_t(
        ipvs->second.cooked,
        info->second.version,
        info->second.map
    );
}

namespace {

void
copy_address(union nf_inet_addr& target, const tcp::endpoint& endpoint) {
    switch(endpoint.protocol().family()) {
    case PF_INET: {
        auto source = reinterpret_cast<const sockaddr_in*>(endpoint.data());
        std::memcpy(&target.in, &source->sin_addr, sizeof(in_addr));
    } break;

    case PF_INET6: {
        auto source = reinterpret_cast<const sockaddr_in6*>(endpoint.data());
        std::memcpy(&target.in6, &source->sin6_addr, sizeof(in6_addr));
    }}
}

}

void
ipvs_t::consume(const std::string& uuid, const std::string& name, const metadata_t& meta) {
    COCAINE_LOG_DEBUG(m_log, "updating node '%s' service '%s'", uuid, name);

    ipvs_dest_t backend;

    if(m_remote_services_ip4.find(name) == m_remote_services_ip4.end() && 
       m_remote_services_ip6.find(name) == m_remote_services_ip6.end()) {
        service_info_t info = {
            std::get<1>(meta), /* protocol version */
            std::get<2>(meta)  /* dispatch map */
        };

        try {
            add_service(name, info);
        } catch(const std::system_error& e) {
            COCAINE_LOG_ERROR(m_log, "unable to create virtual service '%s' - [%d] %s", name,
                e.code().value(), e.code().message());

            return;
        }
    }

    // Setup the backend destination

    std::string hostname;
    uint16_t    port;

    std::tie(hostname, port) = std::get<0>(meta);

    std::vector<io::tcp::endpoint> endpoints;

    try {
        endpoints = io::resolver<io::tcp>::query(
            //m_remote_services[name].endpoint.protocol(),
            hostname,
            port
        );
    } catch(const std::system_error& e) {
        COCAINE_LOG_WARNING(m_log, "unable to resolve any endpoints for service '%s' on '%s' - [%d] %s",
            name, hostname, e.code().value(), e.code().message());

        return;
    }

    for(auto endpoint = endpoints.begin(); endpoint != endpoints.end(); ++endpoint) {
        std::memset(&backend, 0, sizeof(ipvs_dest_t));

        copy_address(backend.addr, *endpoint);

        backend.af         = endpoint->protocol().family();
        if (backend.af != PF_INET) {
            continue;
        }    
        backend.conn_flags = IP_VS_CONN_F_MASQ;
        backend.port       = htons(endpoint->port());
        backend.weight     = m_default_weight;

        try {
            add_backend(name, uuid, backend);
        } catch(const std::system_error& e) {
            COCAINE_LOG_WARNING(m_log, "unable to add endpoint '%s' to virtual service '%s' - [%d] %s",
                *endpoint, name, e.code().value(), e.code().message());

            continue;
        }

        break;
    }

    for(auto endpoint = endpoints.begin(); endpoint != endpoints.end(); ++endpoint) {
        std::memset(&backend, 0, sizeof(ipvs_dest_t));

        copy_address(backend.addr, *endpoint);

        backend.af         = endpoint->protocol().family();
        if (backend.af != PF_INET6) {
            continue;
        } 
        backend.conn_flags = IP_VS_CONN_F_MASQ;
        backend.port       = htons(endpoint->port());
        backend.weight     = m_default_weight;

        try {
            add_backend(name, uuid, backend);
        } catch(const std::system_error& e) {
            COCAINE_LOG_WARNING(m_log, "unable to add endpoint '%s' to virtual service '%s' - [%d] %s",
                *endpoint, name, e.code().value(), e.code().message());

            continue;
        }

        break;
    }
}

void
ipvs_t::cleanup(const std::string& uuid, const std::string& name) {
    COCAINE_LOG_DEBUG(m_log, "cleaning up node '%s' service '%s'", uuid, name);

    try {
        pop_backend(name, uuid);
  
        // Drop the service info, so that it won't lay around stale.
        if((m_remote_services_ip4.count(name) == 0 || m_remote_services_ip4[name].backends.empty()) 
        && (m_remote_services_ip6.count(name) == 0 || m_remote_services_ip6[name].backends.empty())) {
            pop_service(name, m_remote_services_ip4);
            pop_service(name, m_remote_services_ip6);
            m_service_info.erase(name);
        }

    } catch(const std::system_error& e) {
        COCAINE_LOG_ERROR(m_log, "unable to cleanup services - [%d] %s", e.code().value(), e.code().message());
    }
}

void
ipvs_t::add_service(const std::string& name, const service_info_t& info) {
    ipvs_service_t service;

    // Setup the virtual service

    const auto endpoints = io::resolver<io::tcp>::query(
        m_context.config.network.hostname,
        m_ports.top()
    );

    int rv = 0;

    for(auto endpoint = endpoints.begin(); endpoint != endpoints.end(); ++endpoint) {
        std::memset(&service, 0, sizeof(service));

        copy_address(service.addr, *endpoint);
        
        if(endpoint->protocol().family() != PF_INET) {
            continue;
        }
        
        service.af         = endpoint->protocol().family();
        service.port       = htons(endpoint->port());
        service.protocol   = IPPROTO_TCP;

        std::strncpy(service.sched_name, m_default_scheduler.c_str(), IP_VS_SCHEDNAME_MAXLEN);

        if((rv = ::ipvs_add_service(&service)) != 0) {
            std::error_code ec(errno, ipvs_category());

            COCAINE_LOG_ERROR(m_log, "unable to create virtual service '%s' on '%s' - [%d] %s", name,
                *endpoint, ec.value(), ec.message());

            continue;
        }

        COCAINE_LOG_INFO(m_log, "creating virtual service '%s' on '%s'", name, *endpoint);

        // Store the virtual service information

        m_remote_services_ip4[name] = remote_service_t { service, *endpoint, locator::endpoint_tuple_type(
            m_context.config.network.hostname,
            endpoint->port()
        )};

        break;
    }

    for(auto endpoint = endpoints.begin(); endpoint != endpoints.end(); ++endpoint) {
        std::memset(&service, 0, sizeof(service));

        copy_address(service.addr, *endpoint);
        
        // Chinese people do that in ipvsadm probably for a reason.
        if(endpoint->protocol().family() == PF_INET6) {
            service.netmask = 128;
        } else {
            continue;
        }
        
        service.af         = endpoint->protocol().family();
        service.port       = htons(endpoint->port());
        service.protocol   = IPPROTO_TCP;

        std::strncpy(service.sched_name, m_default_scheduler.c_str(), IP_VS_SCHEDNAME_MAXLEN);

        if((rv = ::ipvs_add_service(&service)) != 0) {
            std::error_code ec(errno, ipvs_category());

            COCAINE_LOG_ERROR(m_log, "unable to create virtual service '%s' on '%s' - [%d] %s", name,
                *endpoint, ec.value(), ec.message());

            continue;
        }

        COCAINE_LOG_INFO(m_log, "creating virtual service '%s' on '%s'", name, *endpoint);

        // Store the virtual service information

        m_remote_services_ip6[name] = remote_service_t { service, *endpoint, locator::endpoint_tuple_type(
            m_context.config.network.hostname,
            endpoint->port()
        )};

        break;
    }

    if(rv != 0) {
        throw std::system_error(std::make_error_code(std::errc::address_not_available));
    }

    m_ports.pop();

    // NOTE: Store the first seen protocol description for the service.
    m_service_info[name] = info;
}

void
ipvs_t::add_backend(const std::string& name, const std::string& uuid, ipvs_dest_t backend) {
    std::map<std::string, remote_service_t>::iterator it;
    if(backend.af == PF_INET) {
         it = m_remote_services_ip4.find(name); 
         BOOST_ASSERT(it != m_remote_services_ip4.end());
    } else {
        it = m_remote_services_ip6.find(name);
        BOOST_ASSERT(it != m_remote_services_ip6.end());
    }
    //auto  it = m_remote_services.find(name);

    //BOOST_ASSERT(it != m_remote_services.end());

    auto& service = it->second;

    if(service.backends.find(uuid) != service.backends.end()) {
        return;
    }

    COCAINE_LOG_INFO(m_log, "adding node '%s' to virtual service '%s'", uuid, name);

    if(::ipvs_add_dest(&service.handle, &backend) != 0) {
        throw std::system_error(errno, ipvs_category());
    }

    service.backends[uuid] = backend;
}

void
ipvs_t::pop_service(const std::string& name, std::map<std::string, remote_service_t>& remote_services) {
    auto  it = remote_services.find(name);

    if(it != remote_services.end()) {
        auto& service = it->second;

        BOOST_ASSERT(service.backends.empty());

        COCAINE_LOG_INFO(m_log, "destroying virtual service '%s'", name);

        if(::ipvs_del_service(&service.handle) != 0) {
            throw std::system_error(errno, ipvs_category());
        }

        m_ports.push(ntohs(service.handle.port));

        // Drop the IPVS state.
        remote_services.erase(it);
    }

}

void
ipvs_t::pop_backend(const std::string& name, const std::string& uuid) {
    auto  it_ip4 = m_remote_services_ip4.find(name);
    auto  it_ip6 = m_remote_services_ip6.find(name);

    if(it_ip4 != m_remote_services_ip4.end()){

        auto& service = it_ip4->second;
        auto  backend = service.backends.find(uuid);

        if(backend == service.backends.end()) {
            return;
        }

        COCAINE_LOG_INFO(m_log, "removing node '%s' from virtual service '%s'", uuid, name);

        if(::ipvs_del_dest(&service.handle, &backend->second) != 0) {
            throw std::system_error(errno, ipvs_category());
        }

        service.backends.erase(backend);
    }

    if(it_ip6 != m_remote_services_ip6.end()){

        auto& service = it_ip6->second;
        auto  backend = service.backends.find(uuid);

        if(backend == service.backends.end()) {
            return;
        }

        COCAINE_LOG_INFO(m_log, "removing node '%s' from virtual service '%s'", uuid, name);

        if(::ipvs_del_dest(&service.handle, &backend->second) != 0) {
            throw std::system_error(errno, ipvs_category());
        }

        service.backends.erase(backend);
    }

}
