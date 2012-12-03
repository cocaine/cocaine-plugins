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

#ifndef COCAINE_LOG_SERVICE_HPP
#define COCAINE_LOG_SERVICE_HPP

#include "cocaine/api/reactor.hpp"
#include "cocaine/api/service.hpp"

namespace cocaine {
    
namespace io {
    namespace tags {
        struct logging_tag;
    }

    namespace service {
        struct emit {
            typedef tags::logging_tag tag;

            typedef boost::mpl::list<
                int,
                std::string,
                std::string
            > tuple_type;
        };
    }

    template<>
    struct dispatch<tags::logging_tag> {
        typedef mpl::list<
            service::emit
        > category;
    };
}

namespace service {

class logging_t:
    public api::service_t,
    public api::reactor<io::tags::logging_tag>
{
    public:
        typedef api::service_t category_type;

    public:
        logging_t(context_t& context,
                  const std::string& name,
                  const Json::Value& args);

        virtual
        void
        run();

        virtual
        void
        terminate();

    private:
        void
        on_emit(int priority,
                std::string source,
                std::string message);

    private:
        context_t& m_context;

#if BOOST_VERSION >= 103600
        typedef boost::unordered_map<
#else
        typedef std::map<
#endif
            std::string,
            boost::shared_ptr<logging::logger_t>
        > log_map_t;

        log_map_t m_logs;
};

}}

#endif
