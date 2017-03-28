/*
* 2015+ Copyright (c) Anton Matveenko <antmat@yandex-team.ru>
* All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*/

#ifndef COCAINE_GRAPHITE_SERVICE_HPP
#define COCAINE_GRAPHITE_SERVICE_HPP

#include "cocaine/idl/graphite.hpp"

#include <cocaine/api/service.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/rpc/dispatch.hpp>

#include <asio/ip/tcp.hpp>
#include <asio/deadline_timer.hpp>


namespace cocaine { namespace service {
class graphite_cfg_t {
public:
    asio::ip::tcp::endpoint endpoint;
    std::string prefix;
    boost::posix_time::milliseconds flush_interval_ms;
    size_t max_queue_size;
    graphite_cfg_t(const cocaine::dynamic_t& args);
};

class graphite_t:
    public api::service_t,
    public dispatch<io::graphite_tag>
{
public:

    graphite_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args);

    void
    on_send_one(const graphite::metric_t& metric);

    void
    on_send_bulk(const graphite::metric_pack_t& metrics);

    virtual
    auto
    prototype() -> io::basic_dispatch_t& {
        return *this;
    }

    void
    send_by_timer(const asio::error_code& error);

private:
    class graphite_sender_t;
    typedef graphite::metric_pack_t buffer_t;

    void send();
    void reset_timer();
    asio::io_service& asio;
    graphite_cfg_t config;
    std::shared_ptr<logging::logger_t> log;
    synchronized<asio::deadline_timer> timer;
    synchronized<buffer_t> buffer;
};
}}
#endif
