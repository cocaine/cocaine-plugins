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

#include "cocaine/postgres/pool.hpp"

#include <asio/deadline_timer.hpp>

namespace cocaine {
namespace sender {

class pg_sender_t : public api::sender_t {
public:
    pg_sender_t(context_t& context,
                asio::io_service& io_service,
                const std::string& name,
                data_provider_ptr data_provider,
                const dynamic_t& args);

private:
    enum class policy_t {
        continous,
        update
    };
    auto on_send_timer(const std::error_code& ec) -> void;
    auto on_gc_timer(const std::error_code& ec) -> void;
    auto send(dynamic_t data) -> void;

    policy_t policy;
    data_provider_ptr data_provider;
    std::shared_ptr<api::postgres::pool_t> pool;
    std::string hostname;
    std::string table_name;
    std::unique_ptr<blackhole::logger_t> logger;
    boost::posix_time::seconds send_period;
    asio::deadline_timer send_timer;
    asio::deadline_timer gc_timer;
    boost::posix_time::seconds gc_period;
    boost::posix_time::seconds gc_ttl;
};

}
}
