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

#include <mutex>

namespace cocaine {
namespace logging {

metafilter_t::metafilter_t(std::unique_ptr<logger_t> _logger) : logger(std::move(_logger)) {}

void metafilter_t::add_filter(filter_info_t filter) {
    std::lock_guard<boost::shared_mutex> guard(mutex);
    auto id = filter.id;
    auto it = std::find_if(filters.begin(), filters.end(), [=](const filter_info_t& info) {
        return info.id == id;
    });
    if(it == filters.end()) {
        filters.push_back(std::move(filter));
        since_change_accepted_cnt.store(0);
        since_change_rejected_cnt.store(0);
    }
}

bool metafilter_t::remove_filter(filter_t::id_t filter_id) {
    std::lock_guard<boost::shared_mutex> guard(mutex);
    auto it = std::find_if(filters.begin(), filters.end(), [=](const filter_info_t& info) {
        return info.id == filter_id;
    });

    if(it == filters.end()) {
        return false;
    } else {
        remove_filter(it);
        return true;
    }
}

std::vector<filter_info_t>::iterator metafilter_t::remove_filter(std::vector<filter_info_t>::iterator it) {
    since_change_accepted_cnt.store(0);
    since_change_rejected_cnt.store(0);
    return filters.erase(it);
}

bool metafilter_t::empty() const {
    boost::shared_lock<boost::shared_mutex> guard(mutex);
    return filters.empty();
}

filter_result_t metafilter_t::apply(blackhole::severity_t severity,
                                    blackhole::attribute_pack& attributes) {
    std::vector<filter_t::id_t> ids_to_remove;
    filter_t::deadline_t now = filter_t::clock_t::now();
    filter_result_t result = filter_result_t::reject;
    boost::shared_lock<boost::shared_mutex> guard(mutex);

    bool need_cleanup = false;
    for (const auto& filter_info : filters) {
        if (now > filter_info.deadline) {
            need_cleanup = true;
        } else if (result == filter_result_t::reject &&
                   filter_info.filter.apply(severity, attributes) ==
                       filter_result_t::accept) {
            result = filter_result_t::accept;
        }
    }
    guard.unlock();
    if(need_cleanup) {
        cleanup();
    }
    if(result == filter_result_t::reject) {
        overall_rejected_cnt++;
        since_change_rejected_cnt++;
    } else {
        overall_accepted_cnt++;
        since_change_accepted_cnt++;
    }

    return result;
}

void metafilter_t::each(const callable_t& fn) const {
    boost::shared_lock<boost::shared_mutex> guard(mutex);
    for (const auto& filter_info : filters) {
        fn(filter_info);
    }
}

void metafilter_t::cleanup() {
    filter_t::deadline_t now = filter_t::clock_t::now();
    std::lock_guard<boost::shared_mutex> guard(mutex);
    for (auto it = filters.begin(); it != filters.end(); ) {
        if (now > it->deadline) {
            it = remove_filter(it);
        } else {
            it++;
        }
    }
}

}
}  // namespace cocaine::logging
