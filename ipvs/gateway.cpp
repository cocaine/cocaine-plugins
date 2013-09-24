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

    for(auto it = m_remote_services.begin(); it != m_remote_services.end(); ++it) {
        ::ipvs_del_service(&it->second.handle);
    }

    ::ipvs_close();
}

resolve_result_type
ipvs_t::resolve(const std::string& name) const {
    const auto ipvs = m_remote_services.find(name);

    if(ipvs == m_remote_services.end()) {
        throw cocaine::error_t("the specified service is not available");
    }

    const auto info = m_service_info.find(name);

    BOOST_VERIFY(info != m_service_info.end());

    COCAINE_LOG_DEBUG(m_log, "providing '%s' using virtual service on '%s'", name, ipvs->second.endpoint);

    return std::make_tuple(
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
ipvs_t::consume(const std::string& uuid, synchronize_result_type dump) {
    COCAINE_LOG_DEBUG(m_log, "updating node '%s' services", uuid);

    for(auto it = dump.cbegin(); it != dump.cend(); ++it) {
        ipvs_dest_t backend;

        if(m_remote_services.find(it->first) == m_remote_services.end()) {
            service_info_t info = {
                std::get<1>(it->second), /* protocol version */
                std::get<2>(it->second)  /* dispatch map */
            };

            try {
                add_service(it->first, info);
            } catch(const std::system_error& e) {
                COCAINE_LOG_ERROR(m_log, "unable to create virtual service '%s' - [%d] %s", it->first,
                    e.code().value(), e.code().message());

                continue;
            }
        }

        // Setup the backend destination

        std::string hostname;
        uint16_t    port;

        std::tie(hostname, port) = std::get<0>(it->second);

        std::vector<io::tcp::endpoint> endpoints;

        try {
            endpoints = io::resolver<io::tcp>::query(
                m_remote_services[it->first].endpoint.protocol(),
                hostname,
                port
            );
        } catch(const std::system_error& e) {
            COCAINE_LOG_WARNING(m_log, "unable to resolve any endpoints for service '%s' on '%s' - [%d] %s",
                it->first, hostname, e.code().value(), e.code().message());

            continue;
        }

        for(auto endpoint = endpoints.begin(); endpoint != endpoints.end(); ++endpoint) {
            std::memset(&backend, 0, sizeof(ipvs_dest_t));

            copy_address(backend.addr, *endpoint);

            backend.af         = endpoint->protocol().family();
            backend.conn_flags = IP_VS_CONN_F_MASQ;
            backend.port       = htons(endpoint->port());
            backend.weight     = m_default_weight;

            try {
                add_backend(it->first, uuid, backend);
            } catch(const std::system_error& e) {
                COCAINE_LOG_WARNING(m_log, "unable to add endpoint '%s' to virtual service '%s' - [%d] %s",
                    *endpoint, it->first, e.code().value(), e.code().message());

                continue;
            }

            break;
        }
    }

    // Cleanup stale backends

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

        try {
            for(auto it = stale.cbegin(); it != stale.cend(); ++it) {
                pop_backend(it->first, uuid);

                if(m_remote_services[it->first].backends.empty()) {
                    pop_service(it->first);
                }
            }
        } catch(const std::system_error& e) {
            COCAINE_LOG_ERROR(m_log, "unable to cleanup services - [%d] %s", e.code().value(), e.code().message());
        }
    }

    m_history[uuid] = dump;
}

void
ipvs_t::prune(const std::string& uuid) {
    COCAINE_LOG_DEBUG(m_log, "pruning node '%s' services", uuid);

    std::vector<std::string> names;

    std::transform(
        m_remote_services.begin(),
        m_remote_services.end(),
        std::back_inserter(names),
        tuple::nth_element<0>()
    );

    try {
        for(auto it = names.cbegin(); it != names.cend(); ++it) {
            pop_backend(*it, uuid);

            if(m_remote_services[*it].backends.empty()) {
                pop_service(*it);
            }
        }
    } catch(const std::system_error& e) {
        COCAINE_LOG_ERROR(m_log, "unable to prune services - [%d] %s", e.code().value(), e.code().message());
    }

    m_history.erase(uuid);
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

        m_remote_services[name] = remote_service_t { service, *endpoint, std::make_tuple(
            endpoint->address().to_string(),
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
    auto  it = m_remote_services.find(name);

    BOOST_ASSERT(it != m_remote_services.end());

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
ipvs_t::pop_service(const std::string& name) {
    auto  it = m_remote_services.find(name);

    BOOST_ASSERT(it != m_remote_services.end());

    auto& service = it->second;

    BOOST_ASSERT(service.backends.empty());

    COCAINE_LOG_INFO(m_log, "destroying virtual service '%s'", name);

    if(::ipvs_del_service(&service.handle) != 0) {
        throw std::system_error(errno, ipvs_category());
    }

    m_ports.push(ntohs(service.handle.port));

    // Drop the IPVS state.
    m_remote_services.erase(it);

    // Drop the service info, so that it won't lay around stale.
    m_service_info.erase(name);
}

void
ipvs_t::pop_backend(const std::string& name, const std::string& uuid) {
    auto  it = m_remote_services.find(name);

    BOOST_ASSERT(it != m_remote_services.end());

    auto& service = it->second;
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
