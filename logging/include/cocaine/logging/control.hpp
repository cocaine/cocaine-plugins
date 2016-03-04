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
#include "cocaine/logging/metafilter.hpp"
#include "cocaine/unicorn/api/zookeeper.hpp"
#include <vector>
#include <string>

namespace cocaine { namespace logging {

class control_t {
public:
    struct impl_t;
    struct filter_with_info_t {
        std::shared_ptr<filter_t> filter;
        filter_info_t info;
    };

    control_t();

    std::shared_ptr<metafilter_t>
    metafilter(const std::string& name);

    std::vector<std::string>
    logger_names();

    std::vector<filter_with_info_t>
    filters();

    filter_t::id_type
    add_filter(std::string logger_name, std::shared_ptr<filter_t> filter);

    bool
    remove_filter(filter_t::id_type id);
private:
    std::atomic<filter_t::id_type>
    zookeeper_api_t zk;
    synchronized<std::map<std::string, std::shared_ptr<metafilter_t>>> metafilters;
};

template<>
struct category_traits<control_t> {
    typedef std::shared_ptr<control_t> ptr_type;

    struct factory_type: public api::basic_factory<control_t> {
        virtual
        ptr_type
        get() = 0;
    };

    template<class T>
    struct default_factory: public factory_type {
        virtual
        ptr_type
        get() {
            return instance.apply([](std::shared_ptr<control_t>& instance){
                if (!instance) {
                    instance = new control_t();
                }
                return instance;
            });
        }

    private:
        synchronized<std::shared_ptr<control_t>> instance;
    };
};

}} // namespace cocaine::logging
