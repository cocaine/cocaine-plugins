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

#ifndef COCAINE_BLASTBEAT_DRIVER_HPP
#define COCAINE_BLASTBEAT_DRIVER_HPP

#include <cocaine/common.hpp>

// Has to be included after common.hpp
#include <ev++.h>

#include "cocaine/io.hpp"

#include "cocaine/api/driver.hpp"

namespace cocaine { namespace driver {

class blastbeat_t:
    public api::driver_t
{
    public:
        typedef api::driver_t category_type;

    public:
        blastbeat_t(context_t& context,
                    engine::engine_t& engine,
                    const std::string& name,
                    const Json::Value& args);

        virtual
        ~blastbeat_t();

        // Driver interface.
        virtual
        Json::Value info() const;

    private:
        // Socket event handlers.
        void
        event(ev::io&, int);
        
        void
        process(ev::idle&, int);
        
        void
        check(ev::prepare&, int);

    private:
        // Blastbeat event handlers.
        void
        on_ping();
        
        void
        on_spawn();
        
        void
        on_uwsgi(const std::string& sid,
                 zmq::message_t& message);
        
        void
        on_body(const std::string& sid, 
                zmq::message_t& message);

        void
        on_end(const std::string& sid); 
    
    protected:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        std::string m_event;

        // Blastbeat configuration.
        std::string m_identity;
        std::string m_endpoint;

        ev::io m_watcher; 
        ev::idle m_processor;
        ev::prepare m_check;
        
        io::socket_t m_socket;
};

}}

#endif
