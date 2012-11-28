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

#ifndef COCAINE_SERVER_HPP
#define COCAINE_SERVER_HPP

#include "cocaine/common.hpp"
#include "cocaine/asio.hpp"
#include "cocaine/auth.hpp"
#include "cocaine/io.hpp"

#include "cocaine/api/service.hpp"

#include "cocaine/helpers/json.hpp"

namespace cocaine { namespace service {

class server_t:
    public api::service_t
{
    public:
        typedef api::service_t category_type;

    public:
        server_t(context_t& context,
                 const std::string& name,
                 const Json::Value& args);

        ~server_t();

        virtual
        void
        run();

        virtual
        void
        terminate();

    private:        
        void
        on_terminate(ev::sig&, int);
        
        void
        on_reload(ev::sig&, int);

        void
        on_event(ev::io&, int);
        
        void
        on_check(ev::prepare&, int);

        void
        on_announce(ev::timer&, int);

        void
        process();
        
        std::string
        dispatch(const Json::Value& root);

        Json::Value
        create_app(const std::string& name,
                   const std::string& profile);

        Json::Value
        delete_app(const std::string& name);

        Json::Value
        info() const;

        void
        recover();

    private:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        // I/O
        
        typedef io::socket<
            io::policies::unique
        > socket_t;

        socket_t m_server;

        std::unique_ptr<socket_t> m_announces;
        
        // Event loop
        
        ev::default_loop m_loop;

        ev::sig m_sigint,
                m_sigterm,
                m_sigquit, 
                m_sighup;
                
        ev::io m_watcher;
        ev::prepare m_checker;

        std::unique_ptr<ev::timer> m_announce_timer;

        // Apps
        
        const std::string m_runlist;

#if BOOST_VERSION >= 103600
        typedef boost::unordered_map<
#else
        typedef std::map<
#endif
            const std::string,
            boost::shared_ptr<app_t>
        > app_map_t;

        app_map_t m_apps;

        // Authorization subsystem
        
        crypto::auth_t m_auth;
        
        // Server info
        
        const ev::tstamp m_birthstamp,
                         m_announce_interval;

        ev::tstamp m_infostamp;
        std::string m_infocache;
};

}} // namespace cocaine::service

#endif
