/*
* 2015+ Copyright (c) Dmitry Unkovsky <diunko@yandex-team.ru>
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

#include "cocaine/conductor.hpp"
#include <cocaine/dynamic.hpp>
#include <cocaine/context.hpp>

#include <blackhole/scoped_attributes.hpp>

namespace ph = std::placeholders;


using namespace blackhole;

namespace cocaine { namespace service {


conductor_t::conductor_t(context_t& context, asio::io_service& _asio, const std::string& name, const dynamic_t& args) :
    category_type(context, _asio, name, args),
    dispatch<io::conductor_tag>(name),
    log(context.log("conductor")),
    m_request_id(123)
{
    //on<io::conductor::subscribe>(std::bind(&contuctor_t::on_subscribe, this));
    //on<io::cache::get>(std::bind(&cache_t::get, this, ph::_1));
    on<io::conductor::subscribe>(std::bind(&conductor_t::on_subscribe, this, ph::_1));
    on<io::conductor::spool_done>(std::bind(&conductor_t::on_spool_done, this, ph::_1, ph::_2, ph::_3));
    on<io::conductor::spawn_done>(std::bind(&conductor_t::on_spawn_done, this, ph::_1, ph::_2, ph::_3));
}

void
conductor_t::on_spool_done(uint64_t request_id, const std::error_code& ec, const std::string& error_message)
{
    COCAINE_LOG_DEBUG(log, "spool done: (id: %d, ec: %s)", request_id, ec.message());
}

void
conductor_t::on_spawn_done(uint64_t request_id, const std::error_code& ec, const std::string& error_message)
{
    COCAINE_LOG_DEBUG(log, "spawn_done: (id: %d, ec: %s)", request_id, ec.message());
    m_actions_waiting[request_id]->handler(std::error_code());
    COCAINE_LOG_DEBUG(log, "spawn_done end: (id: %d, ec: %s)", request_id, ec.message());
}

auto
conductor_t::on_subscribe(uint64_t client_id) -> streamed<subscribe_result_t> {
    streamed<subscribe_result_t> stream;

    scoped_attributes_t attributes(*log, { attribute::make("agent", client_id) });

    auto mapping = m_remotes.synchronize();

    if(!mapping->erase(client_id)) {
        COCAINE_LOG_INFO(log, "attaching an outgoing conductor stream for agent %d", client_id);
    }

    mapping->insert({client_id, stream});


    requests_map_t::iterator it;


    for(auto it = m_requests_pending.begin(); it != m_requests_pending.end();){

        uint64_t id = it->first;
        request_ptr_t rq(std::move(it->second));

        stream.write(id, *rq);
        m_requests_waiting.insert(std::pair<uint64_t, request_ptr_t>(id, std::move(rq)));

        it = m_requests_pending.erase(it);

    }

    return stream;
    
    //return stream.write(m_cfg.uuid, m_snapshots);
}

// auto
// conductor_t::on_subscribe(const std::string& uuid) -> streamed<results::connect> {
//     streamed<subscribe_result_t> stream;

//     auto mapping = m_remotes.synchronize();

//     if(!mapping->erase(uuid)) {
//         COCAINE_LOG_INFO(log, "attaching an outgoing stream for conductor");
//     }

//     mapping->insert({uuid, stream});

//     return stream;
// }

void
conductor_t::spool(std::string& image, std::function<void(const std::error_code&)> handler){

    //COCAINE_LOG_DEBUG(log, "spool(%s)", image);
    COCAINE_LOG_DEBUG(log, "================ spool ================ %s", image);
    // std::string request_id = unique_id_t().string();

    uint64_t rq_id = m_request_id++;
    std::ostringstream rq_id_s;

    rq_id_s << rq_id;

    request_map_t spool_request = {{ "request", {{"id", rq_id_s.str()}, {"action", "spool"}, {"image", image}}}};

    auto mapping = m_remotes.synchronize();

    for (auto it = mapping->begin(); it != mapping->end();) try {
        COCAINE_LOG_DEBUG(log, "write spool request %d to remote agent[%d]", rq_id, it->first);
        it->second.write(rq_id, spool_request);
        ++it;
    } catch (...){
        COCAINE_LOG_ERROR(log, "write spool request %d to remote agent[%d] failed", rq_id, it->first);
        it = mapping->erase(it);
    }
    handler(std::error_code());
}

void
conductor_t::spawn(std::string image,
                   std::string path,
                   api::string_map_t&& isolate_params,
                   api::string_map_t&& args,
                   api::string_map_t&& environment,
                   std::function<void(const std::error_code&)> handler)
{
    COCAINE_LOG_DEBUG(log, "================ spawn ================ %s / %s", image, path);

    uint64_t rq_id = m_request_id++;
    std::ostringstream rq_id_s;

    rq_id_s << rq_id;

    request_map_t spawn_request = {
        {"request",
         {{"id", rq_id_s.str()},
          {"action", "spawn"},
          {"image", image}}},
        {"args", args},
        {"environment", environment},
        {"isolate_params", isolate_params}};

    auto mapping = m_remotes.synchronize();

    for (auto it = mapping->begin(); it != mapping->end();) try {
        COCAINE_LOG_DEBUG(log, "writing spawn request %s to the outgoing stream [%s]", rq_id, it->first);
        it->second.write(rq_id, spawn_request);
        ++it;
    } catch (...){
        COCAINE_LOG_ERROR(log, "write spawn request %s to the outgoing stream [%s] failed", rq_id, it->first);
        it = mapping->erase(it);
    }

    m_actions_waiting[rq_id] = std::make_unique<spawn_action_t>(rq_id, handler);

    //handler(std::error_code());
}

}}
