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

#include "cocaine/logging/attribute.hpp"

#include <blackhole/attributes.hpp>
#include <blackhole/message.hpp>
#include <blackhole/severity.hpp>

#include <stdint.h>
#include <string>
#include <chrono>

namespace cocaine { namespace logging {

enum class filter_result_t {
    accept,
    reject
};

class filter_t {
public:
    typedef uint64_t id_type;
    typedef dynamic_t representation_t;
    //TODO: FIXME, use proper type
    enum class disposition_t {
        local,
        cluster
    };
    typedef std::chrono::steady_clock::time_point deadline_t;

    filter_result_t
    apply(const std::string& message, unsigned int severity, const logging::attributes_t& attributes) const;

    filter_t();

    filter_t(const representation_t& representation);

    representation_t
    representation() const;

    class filter_impl_t;

    struct deleter_t {
        void operator()(filter_impl_t*);
    };

private:
    std::unique_ptr<filter_impl_t, deleter_t> impl;
};

struct filter_info_t {
    filter_info_t(filter_t _filter,
                  filter_t::deadline_t _deadline,
                  filter_t::id_type _id,
                  filter_t::disposition_t _disposition,
                  std::string _logger_name);

    filter_info_t(const dynamic_t& value);

    filter_t filter;
    filter_t::deadline_t deadline;
    filter_t::id_type id;
    filter_t::disposition_t disposition;
    std::string logger_name;

    dynamic_t
    representation();
};

}} // namesapce cocaine::logging
