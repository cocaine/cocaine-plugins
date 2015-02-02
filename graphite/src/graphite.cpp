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

#include "cocaine/graphite.hpp"
#include <cocaine/dynamic.hpp>
#include <cocaine/context.hpp>

namespace cocaine { namespace service {

class graphite_t::graphite_sender_t :
    public std::enable_shared_from_this<graphite_t::graphite_sender_t>
{
public:
    graphite_sender_t(graphite_t* parent, graphite_t::buffer_t&& buffer);
    void send();
    void on_connect(const asio::error_code& ec);
    void on_send(const asio::error_code& ec);
private:
    graphite_t* parent;
    graphite_t::buffer_t buffer;
    std::string s_buffer;
    asio::ip::tcp::socket socket;
};

graphite_t::graphite_sender_t::graphite_sender_t(graphite_t* _parent, graphite_t::buffer_t&& _buffer) :
    parent(_parent),
    buffer(std::move(_buffer)),
    s_buffer(),
    socket(parent->asio)
{
}

void graphite_t::graphite_sender_t::send() {
    using namespace std::placeholders;
    socket.async_connect(parent->config.endpoint, std::bind(&graphite_sender_t::on_connect, shared_from_this(), _1));
}

void graphite_t::graphite_sender_t::on_connect(const asio::error_code& ec) {
    if(ec) {
        COCAINE_LOG_WARNING(parent->log,
            "Could not send %lli metrics to graphite. Could not open socket:%s",
            buffer.size(),
            ec.message().c_str()
        );
    }
    else {
        COCAINE_LOG_DEBUG(parent->log, "Opened socket to send metrics to graphite");
        assert(s_buffer.empty());
        for(size_t i = 0; i < buffer.size(); i++) {
            s_buffer.append(buffer[i].format());
        }
        using namespace std::placeholders;
        socket.async_send(asio::buffer(s_buffer), std::bind(&graphite_sender_t::on_send, shared_from_this(), _1));
    }
}

void graphite_t::graphite_sender_t::on_send(const asio::error_code& ec) {
    if(ec) {
        COCAINE_LOG_WARNING(parent->log,
            "Could not send %lli metrics to graphite. Could not send:%s",
            buffer.size(),
            ec.message().c_str()
        );
    }
    else {
        COCAINE_LOG_DEBUG(parent->log, "Successfully sent %lli metrics", buffer.size());
    }
}

graphite_cfg_t::graphite_cfg_t(const cocaine::dynamic_t& args) :
    endpoint(
        asio::ip::address::from_string(args.as_object().at("endpoint", "127.0.0.1").as_string()),
        args.as_object().at("port", 2003u).as_uint()
    ),
    flush_interval_ms(args.as_object().at("flush_interval_ms", 1000u).as_uint()),
    max_queue_size(args.as_object().at("max_queue_size", 1000u).as_uint())
{}

graphite_t::graphite_t(context_t& context, asio::io_service& _asio, const std::string& name, const dynamic_t& args) :
    service_t(context, _asio, name, args),
    dispatch<io::graphite_tag>(name),
    asio(_asio),
    config(args),
    log(context.log("graphite")),
    timer(asio),
    buffer()
{
    using namespace std::placeholders;
    on<io::graphite::send_bulk>(std::bind(&graphite_t::on_send_bulk, this, _1));
    on<io::graphite::send_one>(std::bind(&graphite_t::on_send_one, this, _1));
    reset_timer();
}

void graphite_t::on_send_one(const graphite::metric_t& metric) {
    size_t buffer_sz;
    {
        auto ptr = buffer.synchronize();
        ptr->push_back(metric);
        buffer_sz = ptr->size();
    }
    if(buffer_sz > config.max_queue_size) {
        COCAINE_LOG_DEBUG(log, "Sending metrics by queue size");
        send();
    }
}

void graphite_t::on_send_bulk(const graphite::metric_pack_t& metrics) {
    size_t buffer_sz;
    {
        auto ptr = buffer.synchronize();
        ptr->insert(ptr->end(), metrics.begin(), metrics.end());
        buffer_sz = ptr->size();
    }
    if(buffer_sz > config.max_queue_size) {
        COCAINE_LOG_DEBUG(log, "Sending metrics by queue size");
        send();
    }
}

void graphite_t::send_by_timer(const asio::error_code& error) {
    if(error) {
        //Do not reset timer as it was reset by send
        COCAINE_LOG_DEBUG(log, "Timer: %s", error.message().c_str());
    }
    else {
        COCAINE_LOG_DEBUG(log, "Sending metrics by timer");
        send();
    }
}

void graphite_t::send() {
    buffer_t tmp_buffer;
    buffer.synchronize()->swap(tmp_buffer);
    if(!tmp_buffer.empty()) {
        auto sender = std::make_shared<graphite_sender_t>(this, std::move(tmp_buffer));
        sender->send();
    }
    reset_timer();
}

void graphite_t::reset_timer() {
    auto ptr = timer.synchronize();
    ptr->expires_from_now(config.flush_interval_ms);
    ptr->async_wait(std::bind(&graphite_t::send_by_timer, this, std::placeholders::_1));
}

}}