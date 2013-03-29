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

#ifndef COCAINE_DRIFTING_TIMER_DRIVER_HPP
#define COCAINE_DRIFTING_TIMER_DRIVER_HPP

#include "recurring_timer.hpp"

#include <cocaine/api/stream.hpp>

namespace cocaine { namespace driver {

class drifting_timer_t;

struct drifting_stream_t:
    public api::null_stream_t
{
    drifting_stream_t(drifting_timer_t& driver);

    virtual
    ~drifting_stream_t();

private:
    drifting_timer_t& m_driver;
};

class drifting_timer_t:
    public recurring_timer_t
{
    public:
        drifting_timer_t(context_t& context,
                         reactor_t& reactor,
                         app_t& app,
                         const std::string& name,
                         const Json::Value& args);

        virtual
        Json::Value
        info() const;

        virtual
        void
        reschedule();

        void
        rearm();
};

}}

#endif
