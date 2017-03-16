/*
    Copyright (c) 2011-2015 Anton Matveenko <antmat@yandex-team.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

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

#pragma once

#include "cocaine/logging/filter.hpp"

#include <cocaine/repository.hpp>

#include <boost/thread/shared_mutex.hpp>

#include <atomic>

namespace cocaine {
namespace logging {

/**
 * Class, holding a bunch of filters
 * All filters are join with OR expression -
 * if one of the filter accepts message, message is accepted.
 */
class metafilter_t {
public:
    metafilter_t(std::unique_ptr<logger_t> logger);
    metafilter_t(const metafilter_t&) = delete;
    metafilter_t& operator=(const metafilter_t&) = delete;

    filter_result_t apply(blackhole::severity_t severity,
                          blackhole::attribute_pack& attributes);

    void add_filter(filter_info_t filter);

    bool remove_filter(filter_t::id_t filter_id);

    bool empty() const;

    typedef std::function<void(const logging::filter_info_t&)> callable_t;

    void each(const callable_t& fn) const;

    struct counter_t {
        size_t accepted;
        size_t rejected;
    };

    counter_t since_create();
    counter_t since_last_change();

private:
    std::atomic<size_t> overall_accepted_cnt;
    std::atomic<size_t> overall_rejected_cnt;
    std::atomic<size_t> since_change_accepted_cnt;
    std::atomic<size_t> since_change_rejected_cnt;

    std::unique_ptr<logger_t> logger;
    std::vector<filter_info_t> filters;

    mutable boost::shared_mutex mutex;
};
}
}
