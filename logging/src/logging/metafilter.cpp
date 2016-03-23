/*
    Copyright (c) 2016 Anton Matveenko <antmat@yandex-team.ru>
    Copyright (c) 2016 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/logging/metafilter.hpp"

#include <cocaine/logging.hpp>

#include <blackhole/logger.hpp>

namespace cocaine {
namespace logging {

metafilter_t::metafilter_t(std::unique_ptr<logger_t> _logger) : logger(std::move(_logger)) {}

void metafilter_t::add_filter(filter_info_t filter) {
    COCAINE_LOG_DEBUG(logger, "adding filter with id {}", filter.id);
    std::lock_guard<boost::shared_mutex> guard(mutex);
    filters.push_back(std::move(filter));
}

bool metafilter_t::remove_filter(filter_t::id_type filter_id) {
    std::lock_guard<boost::shared_mutex> guard(mutex);
    auto it = std::remove_if(filters.begin(), filters.end(), [=](const filter_info_t& info) {
        return info.id == filter_id;
    });
    const bool removed = (it != filters.end());
    if (removed) {
        // We can not be really sure that random ids are unique - we need to guarantee it somehow,
        // but as far as we don't generate millions of filter 64 bits random numbers should be ok.
        assert(it == filters.end() - 1);
        filters.pop_back();
    }
    return removed;
}

filter_result_t metafilter_t::apply(const std::string& message,
                                    unsigned int severity,
                                    const logging::attributes_t& attributes) {
    std::vector<filter_t::id_type> ids_to_remove;
    filter_t::deadline_t now = std::chrono::steady_clock::now();
    filter_result_t result = filter_result_t::accept;
    boost::shared_lock<boost::shared_mutex> guard(mutex);

    COCAINE_LOG_DEBUG(logger, "applying metafilter");
    for (const auto& filter_info : filters) {
        if (now > filter_info.deadline) {
            COCAINE_LOG_DEBUG(logger, "removing filter with id {} due to passed deadline");
            ids_to_remove.push_back(filter_info.id);
        } else if (result == filter_result_t::accept &&
                   filter_info.filter.apply(message, severity, attributes) ==
                       filter_result_t::reject) {
            COCAINE_LOG_DEBUG(logger, "rejecting message by filter {}", filter_info.id);
            result = filter_result_t::reject;
        }
    }
    guard.unlock();
    for (auto id : ids_to_remove) {
        COCAINE_LOG_DEBUG(logger, "removing filter with id {}", id);
        if (!remove_filter(id)) {
            COCAINE_LOG_DEBUG(logger, "filter {} has been already removed", id);
        }
    }
    return result;
}

void metafilter_t::apply_visitor(visitor_t& visitor) {
    boost::shared_lock<boost::shared_mutex> guard(mutex);
    for (const auto& filter_info : filters) {
        visitor(filter_info);
    }
}
}
}  // namespace cocaine::logging
