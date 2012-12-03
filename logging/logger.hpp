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

#ifndef COCAINE_REMOTE_LOGGER_HPP
#define COCAINE_REMOTE_LOGGER_HPP

#include "service.hpp"

#include <cocaine/api/client.hpp>
#include <cocaine/api/logger.hpp>

#include <boost/circular_buffer.hpp>

namespace cocaine { namespace logger {

class remote_t:
    public api::logger_t
{
    public:
        typedef api::logger_t category_type;

    public:
        remote_t(context_t& context,
                 const std::string& name,
                 const Json::Value& args);

        virtual
        void
        emit(logging::priorities priority,
             const std::string& source,
             const std::string& message);

    private:
        // NOTE: This is the maximum number of buffered log messages before the
        // logging service will be assumed dead.
        const uint64_t m_watermark;

        api::client<io::tags::logging_tag> m_client;

        typedef boost::tuple<
            logging::priorities,
            std::string,
            std::string
        > log_entry_t;

        // NOTE: Store the most recent 'watermark' log messages in a circular
        // buffer so that if the logging service goes away, we could safely
        // dump the cached messages to the failback logger.
        boost::circular_buffer<log_entry_t> m_ring;

        // NOTE: The fallback logger.
        api::logger_ptr_t m_fallback;
};

}} // namespace cocaine::sink

#endif
