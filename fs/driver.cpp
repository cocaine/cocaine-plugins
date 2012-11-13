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

#include <cocaine/context.hpp>
#include <cocaine/engine.hpp>
#include <cocaine/logging.hpp>

#include <cocaine/api/event.hpp>

#include "driver.hpp"

using namespace cocaine;
using namespace cocaine::driver;

fs_t::fs_t(context_t& context,
           const std::string& name,
           const Json::Value& args,
           engine::engine_t& engine):
    category_type(context, name, args, engine),
    m_context(context),
    m_log(context.log(
        (boost::format("app/%1%")
            % name
        ).str()
    )),
    m_path(args.get("path", "").asString()),
    m_watcher(engine.loop())
{
    if(m_path.empty()) {
        throw configuration_error_t("no path has been specified");
    }
    
    m_watcher.set<fs_t, &fs_t::on_event>(this);
    m_watcher.start(m_path.c_str());
}

fs_t::~fs_t() {
    m_watcher.stop();
}

Json::Value
fs_t::info() const {
    Json::Value result;

    result["type"] = "filesystem-monitor";
    result["path"] = m_path;

    return result;
}

void
fs_t::on_event(ev::stat&, int) {
    try {
        engine().enqueue(boost::make_shared<fs_event_t>(m_event));
    } catch(const cocaine::error_t& e) {
        COCAINE_LOG_ERROR(m_log, "unable to schedule an event - %s", e.what());
    }
}

