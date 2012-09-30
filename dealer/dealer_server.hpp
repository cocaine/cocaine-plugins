/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_DEALER_SERVER_DRIVER_HPP
#define COCAINE_DEALER_SERVER_DRIVER_HPP

#include <cocaine/common.hpp>

// Has to be included after common.hpp
#include <ev++.h>

#include <cocaine/io.hpp>
#include <cocaine/job.hpp>

#include <cocaine/api/driver.hpp>

namespace cocaine { namespace driver {

class dealer_server_t:
    public api::driver_t
{
    public:
        typedef api::driver_t category_type;

    public:
        dealer_server_t(context_t& context,
                        engine::engine_t& engine,
                        const std::string& name,
                        const Json::Value& args);

        ~dealer_server_t();

        // Driver interface.
        virtual Json::Value info() const;
        
    private:
        typedef boost::tuple<
            std::string&,
            engine::policy_t&,
            zmq::message_t*
        > request_proxy_t;

        void event(ev::io&, int);
        void process(ev::idle&, int);
        void check(ev::prepare&, int);

    private:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        const std::string m_event,
                          m_route;

        ev::io m_watcher;
        ev::idle m_processor;
        ev::prepare m_check;

        // Server RPC channel.        
        io::channel_t m_channel;
};

}}

#endif
