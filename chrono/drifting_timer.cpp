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

#include <cocaine/api/event.hpp>

#include "drifting_timer.hpp"

using namespace cocaine;
using namespace cocaine::driver;

drifting_stream_t::drifting_stream_t(drifting_timer_t& driver):
    m_driver(driver)
{ }

drifting_stream_t::~drifting_stream_t() {
    m_driver.rearm();
}

drifting_timer_t::drifting_timer_t(context_t& context,
                                   const std::string& name,
                                   const Json::Value& args,
                                   engine::engine_t& engine):
    recurring_timer_t(context, name, args, engine)
{ }

Json::Value
drifting_timer_t::info() const {
    Json::Value result(recurring_timer_t::info());

    result["type"] = "drifting-timer";

    return result;
}

void
drifting_timer_t::reschedule() {
    m_watcher.stop();

    enqueue(
        api::event_t(m_event),
        boost::make_shared<drifting_stream_t>(boost::ref(*this))
    );
}

void
drifting_timer_t::rearm() {
    m_watcher.again();
}

