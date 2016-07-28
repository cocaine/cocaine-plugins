/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#include <cocaine/service/node/profile.hpp>
#include "cocaine/service/node/profile.hpp"

#include <cocaine/defaults.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/traits/dynamic.hpp>

namespace cocaine {

profile_t::profile_t(context_t& context, const std::string& name_):
    cached<dynamic_t>(context, "profiles", name_),
    name(name_)
{
    const auto& config = as_object();

    log_output = as_object().at("log-output", false).as_bool();

    timeout.spawn     = static_cast<uint64_t>(1000 * config.at("spawn-timeout", 1.0).to<double>());
    timeout.handshake = static_cast<uint64_t>(1000 * config.at("handshake-timeout", 5.0).to<double>());
    timeout.heartbeat = static_cast<uint64_t>(1000 * config.at("heartbeat-timeout", 30.0f).to<double>());
    timeout.seal      = static_cast<uint64_t>(1000 * config.at("seal-timeout", 60.0).to<double>());
    timeout.terminate = static_cast<uint64_t>(1000 * config.at("terminate-timeout", 10.0).to<double>());
    timeout.idle      = static_cast<uint64_t>(1000 * config.at("idle-timeout", 600.0f).to<double>());
    //timeout.request   = static_cast<uint64_t>(1000 * config.at("request-timeout", 86400.0f).to<double>());

    concurrency         = as_object().at("concurrency", 10L).to<uint64_t>();
    crashlog_limit      = as_object().at("crashlog-limit", 50L).to<uint64_t>();
    pool_limit          = as_object().at("pool-limit", 10L).to<uint64_t>();
    queue_limit         = as_object().at("queue-limit", 100L).to<uint64_t>();

    unsigned long default_threshold = std::max(1UL, queue_limit / pool_limit / 2);

    grow_threshold      = as_object().at("grow-threshold", default_threshold).to<uint64_t>();

    // Isolation

    const auto isolate_config = as_object().at("isolate", dynamic_t::empty_object).as_object();

    isolate.type = isolate_config.at("type", "process").as_string();
    isolate.args = isolate_config.at("args", dynamic_t::empty_object);

    // Validation

    if(timeout.heartbeat == 0) {
        throw cocaine::error_t("slave heartbeat timeout must be positive");
    }

    if(pool_limit == 0) {
        throw cocaine::error_t("engine pool limit must be positive");
    }

    if(concurrency == 0) {
        throw cocaine::error_t("engine concurrency must be positive");
    }
}

unsigned long
profile_t::request_timeout() const {
    return static_cast<uint64_t>(1000 * as_object().at("request-timeout", 86400.0f).to<double>());
}

} //  namespace cocaine
