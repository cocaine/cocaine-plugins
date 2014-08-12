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

#include <cstring>

#include <boost/asio/ip/host_name.hpp>

#include <netinet/in.h>

extern "C" {

#include "libipvs-1.25/libipvs.h"

}

using namespace boost::asio;
using namespace boost::asio::ip;

using namespace cocaine;
using namespace cocaine::api;
using namespace cocaine::gateway;

// IPVS gateway internals

namespace {

class ipvs_category_t:
    public boost::system::error_category
{
    virtual
    auto
    name() const throw() -> const char* {
        return "cocaine.gateway.ipvs";
    }

    virtual
    auto
    message(int code) const -> std::string {
        return ::ipvs_strerror(code);
    }
};

auto
ipvs_category() -> const boost::system::error_category& {
    static ipvs_category_t instance;
    return instance;
}

} // namespace

class ipvs_t::remote_t {
    COCAINE_DECLARE_NONCOPYABLE(remote_t)

    ipvs_t* impl;

    // PF -> Kernel IPVS virtual service handle.
    std::map<int, ipvs_service_t> services;

    // Backend UUID -> kernel IPVS destination handle mapping.
    std::multimap<std::string, ipvs_dest_t> backends;

    // Service metadata: endpoints, version and protocol.
    metadata_t meta;

public:
    remote_t(ipvs_t* impl, int version, const io::dispatch_graph_t& graph);
   ~remote_t();

    // Observers

    auto
    to_metadata() const -> metadata_t;

    // Modifiers

    void
    insert(const std::string& uuid, const std::vector<tcp::endpoint>& endpoints);

    void
    remove(const std::string& uuid);

private:
    void
    format_address(union nf_inet_addr& target, const tcp::endpoint& endpoint);
};

ipvs_t::remote_t::remote_t(ipvs_t* impl_, int version, const io::dispatch_graph_t& graph):
    impl(impl_),
    meta({}, version, graph)
{
    auto  port = rand();
    auto& endpoints = std::get<0>(meta);

    ipvs_service_t handle;

    for(auto it = impl->m_endpoints.begin(); it != impl->m_endpoints.end(); ++it) {
        tcp::endpoint endpoint(*it, port);

        if(services.count(endpoint.protocol().family())) {
            // If there's already a service for that PF, skip the endpoint.
            continue;
        }

        // Cleanup the handle structure every iteration for additional safety.
        std::memset(&handle, 0, sizeof(handle));

        format_address(handle.addr, endpoint);

        handle.af         = endpoint.protocol().family();
        handle.netmask    = endpoint.address().is_v4() ? 32 : 128;
        handle.port       = htons(endpoint.port());
        handle.protocol   = IPPROTO_TCP;

        std::strncpy(handle.sched_name, impl->m_cfg.scheduler.c_str(), IP_VS_SCHEDNAME_MAXLEN);

        if(::ipvs_add_service(&handle) != 0) {
            COCAINE_LOG_WARNING(impl->m_log, "unable to configure virtual service on %s: [%d] %s",
                endpoint, errno, ::ipvs_strerror(errno)
            );

            continue;
        }

        // Store the endpoint.
        endpoints.push_back(endpoint);

        // Store the kernel service handle.
        services[handle.af] = handle;
    }

    if(services.empty()) {
        COCAINE_LOG_ERROR(impl->m_log, "unable to configure virtual service");
    
        // Force disconnect the remote node.
        throw boost::system::system_error(EADDRNOTAVAIL, boost::system::generic_category());
    }
}

ipvs_t::remote_t::~remote_t() {
    for(auto it = backends.begin(); it != backends.end(); ++it) {
        auto& backend = it->second;

        // Destroy the destination using the corresponding service handle.
        ::ipvs_del_dest(&services.at(backend.af), &backend);
    }

    for(auto it = services.begin(); it != services.end(); ++it) {
        ::ipvs_del_service(&it->second);
    }
}

namespace {

struct is_same_pf {
    bool
    operator()(const std::pair<std::string, ipvs_dest_t>& pair) const {
        return pair.second.af == endpoint.protocol().family();
    }

    const tcp::endpoint& endpoint;
};

struct is_serving {
    bool
    operator()(const tcp::endpoint& endpoint) const {
        return std::any_of(backends.begin(), backends.end(), is_same_pf{endpoint});
    }

    const std::multimap<std::string, ipvs_dest_t>& backends;
};

} // namespace

auto
ipvs_t::remote_t::to_metadata() const -> metadata_t {
    if(backends.empty()) {
        throw boost::system::system_error(error::service_not_available);
    }

    auto endpoints = std::vector<tcp::endpoint>();
    auto builder   = std::back_inserter(endpoints);

    std::copy_if(std::get<0>(meta).begin(), std::get<0>(meta).end(), builder,
        is_serving{backends}
    );

    return metadata_t { endpoints, std::get<1>(meta), std::get<2>(meta) };
}

void
ipvs_t::remote_t::insert(const std::string& uuid, const std::vector<tcp::endpoint>& endpoints) {
    ipvs_dest_t handle;

    std::map<int, ipvs_dest_t> handles;

    for(auto it = endpoints.begin(); it != endpoints.end(); ++it) {
        if(!services.count(it->protocol().family()) || handles.count(it->protocol().family())) {
            // If there's no virtual service for that PF or there's already a backend for that
            // PF, skip the endpoint.
            continue;
        }

        // Cleanup the handle structure every iteration for additional safety.
        std::memset(&handle, 0, sizeof(ipvs_dest_t));

        format_address(handle.addr, *it);

        handle.af         = it->protocol().family();
        handle.conn_flags = IP_VS_CONN_F_MASQ;
        handle.port       = htons(it->port());
        handle.weight     = impl->m_cfg.weight;

        if(::ipvs_add_dest(&services.at(handle.af), &handle) != 0) {
            COCAINE_LOG_WARNING(impl->m_log, "unable to configure destination with %s: [%d] %s",
                *it, errno, ipvs_strerror(errno)
            );

            continue;
        }

        handles.insert({handle.af, handle});
    }

    if(handles.empty()) {
        COCAINE_LOG_ERROR(impl->m_log, "unable to configure destination");

        // Force disconnect the remote node.
        throw boost::system::system_error(EHOSTUNREACH, boost::system::generic_category());
    }

    for(auto it = handles.begin(); it != handles.end(); ++it) {
        backends.insert({uuid, it->second});
    }
}

void
ipvs_t::remote_t::remove(const std::string& uuid) {
    if(!backends.count(uuid)) {
        return;
    }

    for(auto it = backends.lower_bound(uuid); it != backends.upper_bound(uuid); ++it) {
        auto& backend = it->second;

        if(::ipvs_del_dest(&services.at(backend.af), &backend) != 0) {
            // Force disconnect the remote node.
            throw boost::system::system_error(errno, ipvs_category());
        }
    }

    backends.erase(uuid);
}

void
ipvs_t::remote_t::format_address(union nf_inet_addr& target, const tcp::endpoint& endpoint) {
    if(endpoint.address().is_v4()) {
        auto source = reinterpret_cast<const sockaddr_in* >(endpoint.data());
        std::memcpy(&target.in,  &source->sin_addr,  sizeof(in_addr));
    } else {
        auto source = reinterpret_cast<const sockaddr_in6*>(endpoint.data());
        std::memcpy(&target.in6, &source->sin6_addr, sizeof(in6_addr));
    }
}

// IPVS gateway

namespace cocaine {

template<>
struct dynamic_converter<ipvs_config_t> {
    typedef ipvs_config_t result_type;

    static
    result_type
    convert(const dynamic_t& source) {
        result_type result;

        result.scheduler = source.as_object().at("scheduler", "wlc").as_string();
        result.weight    = source.as_object().at("weight", 1u).as_uint();

        return result;
    }
};

} // namespace cocaine

ipvs_t::ipvs_t(context_t& context, const std::string& name, const dynamic_t& args):
    category_type(context, name, args),
    m_context(context),
    m_log(context.log(name)),
    m_cfg(args.to<ipvs_config_t>())
{
    if(::ipvs_init() != 0) {
        throw boost::system::system_error(errno, ipvs_category(), "unable to initialize IPVS");
    }

    io_service asio;

    tcp::resolver resolver(asio);
    tcp::resolver::iterator begin, end;

    try {
        begin = resolver.resolve(tcp::resolver::query(
            m_context.config.network.hostname, std::string()
        ));
    } catch(const boost::system::system_error& e) {
        std::throw_with_nested(cocaine::error_t("unable to determine local addresses"));
    }

    for(auto it = begin; it != end; ++it) {
        m_endpoints.push_back(it->endpoint().address());
    }

    std::ostringstream stream;
    std::ostream_iterator<ip::address> builder(stream, ", ");

    std::copy(m_endpoints.begin(), m_endpoints.end(), builder);

    COCAINE_LOG_INFO(m_log, "using %d local address(es): %s", m_endpoints.size(), stream.str());

    // Clean up the IPVS table before use.
    ::ipvs_flush();
}

ipvs_t::~ipvs_t() {
    m_remotes.clear();

    // This doesn't clean the IPVS tables. Instead, the plugin cleans them up on construction.
    ::ipvs_close();
}

auto
ipvs_t::resolve(const std::string& name) const -> metadata_t {
    if(!m_remotes.count(name)) {
        throw boost::system::system_error(error::service_not_available);
    }

    COCAINE_LOG_DEBUG(m_log, "providing service using virtual node")(
        "service", name
    );

    return m_remotes.at(name)->to_metadata();
}

void
ipvs_t::consume(const std::string& uuid, const std::string& name, const metadata_t& meta) {
    auto endpoints = std::vector<tcp::endpoint>();
    auto graph     = io::dispatch_graph_t();
    auto version   = 0;

    std::tie(endpoints, version, graph) = meta;

    if(!m_remotes.count(name)) {
        m_remotes[name] = std::make_unique<remote_t>(this, version, graph);
    }

    m_remotes.at(name)->insert(uuid, endpoints);
}

void
ipvs_t::cleanup(const std::string& uuid, const std::string& name) {
    if(!m_remotes.count(name)) {
        return;
    }

    m_remotes.at(name)->remove(uuid);
}
