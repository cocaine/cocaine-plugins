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

#include <blackhole/scope/holder.hpp>

#include <asio/io_service.hpp>
#include <asio/ip/host_name.hpp>
#include <asio/ip/tcp.hpp>

#include <boost/spirit/include/karma_generate.hpp>
#include <boost/spirit/include/karma_list.hpp>
#include <boost/spirit/include/karma_stream.hpp>
#include <boost/spirit/include/karma_string.hpp>

#include <blackhole/logger.hpp>

extern "C" {

#include "libipvs-1.26/libipvs.h"

}

using namespace blackhole;

using namespace asio;
using namespace asio::ip;

using namespace cocaine::gateway;
using namespace cocaine::io;

// IPVS gateway internals

namespace cocaine {
namespace error {
namespace {

class ipvs_category_t:
    public std::error_category
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

} // namespace

auto
ipvs_category() -> const std::error_category& {
    static ipvs_category_t instance;
    return instance;
}

} // namespace error
} // namespace cocaine

class ipvs_t::remote_t {
    COCAINE_DECLARE_NONCOPYABLE(remote_t)

    struct info_t {
        std::string service;
        std::size_t version;
        std::vector<tcp::endpoint> endpoints;
    };

    ipvs_t *const parent;
    info_t        info;

    // PF -> Kernel IPVS virtual service handle mapping.
    std::map<int, ipvs_service_t> services;

    // Backend UUID -> kernel IPVS destination handle mapping.
    std::multimap<std::string, ipvs_dest_t> backends;

public:
    remote_t(ipvs_t* parent, const partition_t& name);
   ~remote_t();

    // Observers

    auto
    reduce() const -> std::vector<tcp::endpoint>;

    // Modifiers

    size_t
    insert(const std::string& uuid, const std::vector<tcp::endpoint>& endpoints);

    size_t
    remove(const std::string& uuid);

private:
    void
    format_address(union nf_inet_addr& target, const tcp::endpoint& endpoint);
};

ipvs_t::remote_t::remote_t(ipvs_t *const parent_, const partition_t& name_):
    parent(parent_),
    info({std::get<0>(name_), std::get<1>(name_), {}})
{
    ipvs_service_t handle;

    scope::holder_t attributes(*parent->m_log, {
        {"service", info.service},
        {"version", info.version}
    });

    auto port = parent->m_context.mapper.assign(cocaine::format("{}@{}:virtual", info.service,
        info.version));
    COCAINE_LOG_DEBUG(parent->m_log, "publishing virtual service");

    for(auto it = parent->m_endpoints.begin(); it != parent->m_endpoints.end(); ++it) {
        tcp::endpoint endpoint(*it, port);

        if(services.count(endpoint.protocol().family())) {
            COCAINE_LOG_DEBUG(parent->m_log, "skipping virtual service endpoint {}", endpoint);

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

        std::strncpy(handle.sched_name, parent->m_cfg.scheduler.c_str(), IP_VS_SCHEDNAME_MAXLEN);

        if(::ipvs_add_service(&handle) != 0) {
            std::error_code ec(errno, ipvs_category());

            COCAINE_LOG_WARNING(parent->m_log, "unable to configure virtual service on {}: [{}] {}",
                endpoint, ec.value(), ec.message()
            );

            continue;
        }

        // Store the endpoint.
        info.endpoints.push_back(endpoint);

        // Store the kernel service handle.
        services[handle.af] = handle;
    }

    if(services.empty()) {
        COCAINE_LOG_ERROR(parent->m_log, "no valid endpoints found for virtual service");

        // Force disconnect the remote node.
        throw std::system_error(EADDRNOTAVAIL, std::system_category());
    }

    COCAINE_LOG_INFO(parent->m_log, "virtual service published on port {} with {} endpoint(s)",
        port, info.endpoints.size()
    );
}

ipvs_t::remote_t::~remote_t() {
    COCAINE_LOG_DEBUG(parent->m_log, "cleaning up virtual service", attribute_list{
        {"service", info.service},
        {"version", info.version}
    });

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
ipvs_t::remote_t::reduce() const -> std::vector<tcp::endpoint> {
    if(backends.empty()) {
        throw std::system_error(error::service_not_available);
    }

    std::vector<tcp::endpoint> endpoints;

    std::copy_if(info.endpoints.begin(), info.endpoints.end(), std::back_inserter(endpoints),
        is_serving{backends}
    );

    return endpoints;
}

size_t
ipvs_t::remote_t::insert(const std::string& uuid, const std::vector<tcp::endpoint>& endpoints) {
    ipvs_dest_t handle;

    scope::holder_t attributes(*parent->m_log, {
        {"service", info.service},
        {"uuid",    uuid        },
        {"version", info.version}
    });

    std::map<int, ipvs_dest_t> handles;

    COCAINE_LOG_DEBUG(parent->m_log, "registering destination");

    for(auto it = endpoints.begin(); it != endpoints.end(); ++it) {
        if(!services.count(it->protocol().family()) || handles.count(it->protocol().family())) {
            COCAINE_LOG_DEBUG(parent->m_log, "skipping destination endpoint {}", *it);

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
        handle.weight     = parent->m_cfg.weight;

        if(::ipvs_add_dest(&services.at(handle.af), &handle) != 0) {
            std::error_code ec(errno, ipvs_category());

            COCAINE_LOG_WARNING(parent->m_log, "unable to register destination for {}: [{}] {}",
                *it, ec.value(), ec.message()
            );

            continue;
        }

        handles.insert({handle.af, handle});
    }

    if(handles.empty()) {
        COCAINE_LOG_ERROR(parent->m_log, "no valid endpoints found for destination");

        // Force disconnect the remote node.
        throw std::system_error(EHOSTUNREACH, std::system_category());
    }

    for(auto it = handles.begin(); it != handles.end(); ++it) {
        backends.insert({uuid, it->second});
    }

    COCAINE_LOG_INFO(parent->m_log, "destination registered with {} endpoint(s)", handles.size());

    return backends.size();
}

size_t
ipvs_t::remote_t::remove(const std::string& uuid) {
    COCAINE_LOG_INFO(parent->m_log, "removing destination with {} endpoint(s)", backends.count(uuid), attribute_list{
        {"service", info.service},
        {"uuid",    uuid        },
        {"version", info.version}
    });

    for(auto it = backends.lower_bound(uuid); it != backends.upper_bound(uuid); ++it) {
        auto& backend = it->second;

        if(::ipvs_del_dest(&services.at(backend.af), &backend) != 0) {
            // Force disconnect the remote node.
            throw std::system_error(errno, ipvs_category());
        }
    }

    backends.erase(uuid); return backends.size();
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
    COCAINE_LOG_INFO(m_log, "initializing IPVS");

    if(::ipvs_init() != 0) {
        throw std::system_error(errno, ipvs_category(), "unable to initialize IPVS");
    }

    port_t min_port, max_port;
    std::tie(min_port, max_port) = m_context.config.network.ports.shared;
    if(min_port == 0 && max_port == 0) {
        throw cocaine::error_t("shared ports must be specified to use IPVS gateway");
    }

    if(m_context.config.network.endpoint.is_unspecified()) {
        io_service asio;

        tcp::resolver resolver(asio);
        tcp::resolver::iterator begin, end;

        try {
            begin = resolver.resolve(tcp::resolver::query(
                m_context.config.network.hostname, std::string()
            ));
        } catch(const std::system_error& e) {
#if defined(HAVE_GCC48)
            std::throw_with_nested(cocaine::error_t("unable to determine local addresses"));
#else
            throw cocaine::error_t("unable to determine local addresses");
#endif
        }

        for(auto it = begin; it != end; ++it) {
            m_endpoints.push_back(it->endpoint().address());
        }
    } else {
        m_endpoints.push_back(m_context.config.network.endpoint);
    }

    std::ostringstream stream;
    std::ostream_iterator<char> builder(stream);

    boost::spirit::karma::generate(builder, boost::spirit::karma::stream % ", ", m_endpoints);

    COCAINE_LOG_INFO(m_log, "using {} local address(es): {}", m_endpoints.size(), stream.str());

    // Clean up the IPVS table before use.
    ::ipvs_flush();
}

ipvs_t::~ipvs_t() {
    m_remotes->clear();

    COCAINE_LOG_INFO(m_log, "shutting down IPVS");

    // This doesn't clean the IPVS tables. Instead, the plugin cleans them up on construction.
    ::ipvs_close();
}

auto
ipvs_t::resolve(const partition_t& name) const -> std::vector<tcp::endpoint> {
    auto ptr = m_remotes.synchronize();

    if(!ptr->count(name)) {
        throw std::system_error(error::service_not_available);
    }

    COCAINE_LOG_DEBUG(m_log, "providing service using virtual node");

    return ptr->at(name)->reduce();
}

size_t
ipvs_t::consume(const std::string& uuid,
                const partition_t& name, const std::vector<tcp::endpoint>& endpoints)
{
    auto ptr = m_remotes.synchronize();

    if(!ptr->count(name)) {
        (*ptr)[name] = std::make_unique<remote_t>(this, name);
    }

    return ptr->at(name)->insert(uuid, endpoints);
}

size_t
ipvs_t::cleanup(const std::string& uuid, const partition_t& name) {
    auto ptr = m_remotes.synchronize();

    if(!ptr->count(name)) {
        return 0;
    }

    return ptr->at(name)->remove(uuid);
}
