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

namespace cocaine { namespace logging {

class metafilter_t {
public:
    metafilter_t(std::unique_ptr<logger_t> logger);
    metafilter_t(const metafilter_t&) = delete;
    metafilter_t& operator=(const metafilter_t&) = delete;

    filter_result_t
    apply(const std::string& message, unsigned int severity, const logging::attributes_t& attributes);

    void
    add_filter(filter_info_t filter);

    bool
    remove_filter(filter_t::id_type filter_id);

    struct visitor_t {
        virtual
        ~visitor_t(){}

        virtual void
        operator()(const filter_info_t& info) = 0;
    };

    void
    apply_visitor(visitor_t& visitor);

private:
    std::unique_ptr<logger_t> logger;
    std::vector<filter_info_t> filters;
    boost::shared_mutex mutex;
};


}}
