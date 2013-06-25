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

#include "cocaine/api/gateway.hpp"

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
        consume(const std::string& uuid, api::synchronize_result_type dump);

        virtual
        void
        prune(const std::string& uuid);

    private:
        void
        merge(remote_service_map_t update);

    private:
        std::unique_ptr<logging::log_t> m_log;

        struct remove_service_t {
            ipvs_service_t handle;
            
            // Backend UUID -> Destination mapping.
            std::map<std::string, ipvs_dest_t> backends;

            // Service info.
            resolve_result_type info;
        };

        typedef std::map<std::string, remove_service_t> remote_service_map_t;

        remote_service_map_t m_remote_services;
};

}} // namespace cocaine::gateway

#endif
