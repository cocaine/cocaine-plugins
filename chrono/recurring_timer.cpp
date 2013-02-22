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

#include "recurring_timer.hpp"

#include <cocaine/context.hpp>
#include <cocaine/engine.hpp>
#include <cocaine/logging.hpp>

#include <cocaine/api/event.hpp>
#include <cocaine/api/stream.hpp>

using namespace cocaine;
using namespace cocaine::driver;
using namespace cocaine::logging;

recurring_timer_t::recurring_timer_t(context_t& context,
                                     const std::string& name,
                                     const Json::Value& args,
                                     engine::engine_t& engine):
    category_type(context, name, args, engine),
    m_context(context),
    m_log(new log_t(context, cocaine::format("app/%s", name))),
    m_event(args["emit"].asString()),
    m_interval(args.get("interval", 0.0f).asInt() / 1000.0f),
    m_watcher(engine.service().loop())
{
    if(m_interval <= 0.0f) {
        throw configuration_error_t("no interval has been specified");
    }

    m_watcher.set<recurring_timer_t, &recurring_timer_t::on_event>(this);
    m_watcher.start(m_interval, m_interval);
}

recurring_timer_t::~recurring_timer_t() {
    m_watcher.stop();
}

Json::Value
recurring_timer_t::info() const {
    Json::Value result;

    result["type"] = "recurring-timer";
    result["interval"] = m_interval;

    return result;
}

void
recurring_timer_t::enqueue(const api::event_t& event,
                           const std::shared_ptr<api::stream_t>& stream)
{
    try {
        engine().enqueue(event, stream);
    } catch(const cocaine::error_t& e) {
        COCAINE_LOG_ERROR(m_log, "unable to enqueue an event - %s", e.what());
    }
}

void
recurring_timer_t::reschedule() {
    enqueue(api::event_t(m_event), std::make_shared<api::null_stream_t>());
}

void
recurring_timer_t::on_event(ev::timer&, int) {
    reschedule();
}

