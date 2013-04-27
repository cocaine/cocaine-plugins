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

#ifndef COCAINE_GATEWAY_SERVICE_HPP
#define COCAINE_GATEWAY_SERVICE_HPP

#include "cocaine/api/service.hpp"

extern "C" {
    #include "libipvs-1.25/libipvs.h"
}

namespace cocaine { namespace service {

using io::reactor_t;

class gateway_t:
    public api::service_t
{
    public:
        gateway_t(context_t& context,
                  reactor_t& reactor,
                  const std::string& name,
                  const Json::Value& args);

        virtual
       ~gateway_t();

    private:
        void
        add_backend(const std::string& name, const std::string& endpoint);

        void
        pop_backend(const std::string& name, const std::string& endpoint);

    private:
        std::unique_ptr<logging::log_t> m_log;

        // Multicast sink.
        std::unique_ptr<io::socket<io::udp>> m_sink;

        typedef std::map<
            std::string,
            ipvs_service_t
        > service_map_t;

        service_map_t m_services;
};

}} // namespace cocaine::service

#endif
