/*
    Copyright (c) 2016+ Anton Matveenko <antmat@me.com>
    Copyright (c) 2016+ Other contributors as noted in the AUTHORS file.

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

#include "cocaine/api/sender.hpp"

#include <metrics/tags.hpp>
#include <metrics/registry.hpp>

#include <cocaine/api/service.hpp>
#include <cocaine/context.hpp>
#include <cocaine/idl/metrics.hpp>
#include <cocaine/rpc/dispatch.hpp>

#include "metrics/fwd.hpp"

namespace cocaine {
namespace service {

class metrics_t :
    public api::service_t,
    public dispatch<io::metrics_tag>
{
public:
    enum class type_t {
        json,
        plain
    };

public:
    metrics_t(context_t& context,
              asio::io_service& asio,
              const std::string& name,
              const dynamic_t& args);

    auto
    prototype() const -> const io::basic_dispatch_t& {
        return *this;
    }

    /// Returns metrics dump.
    auto
    metrics(const std::string& type, const dynamic_t& query) const -> dynamic_t;

private:
    auto
    make_type(const std::string& type) const -> type_t;

    auto
    make_filter(const dynamic_t& query) const -> libmetrics::query_t;

    auto
    construct_plain(const libmetrics::query_t& filter) const -> dynamic_t;

    auto
    construct_dendroid(const libmetrics::query_t& filter) const -> dynamic_t;

private:
    libmetrics::registry_t& hub;
    std::vector<api::sender_ptr> senders;
    std::shared_ptr<metrics::factory_t> factory;
};

}  // namespace service
}  // namespace cocaine
