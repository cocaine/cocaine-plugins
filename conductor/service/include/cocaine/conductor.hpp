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

#ifndef COCAINE_CONDUCTOR_SERVICE_HPP
#define COCAINE_CONDUCTOR_SERVICE_HPP

#include <cocaine/api/isolate.hpp>
#include <cocaine/api/service.hpp>

#include "cocaine/idl/conductor.hpp"
#include <cocaine/rpc/dispatch.hpp>

namespace {

typedef struct {
    std::string image;
    std::string request_id;
} spool_action_t;


}

namespace cocaine { namespace service {

class conductor_t:
    public api::service_t,
    public dispatch<io::conductor_tag>
{

public:

    class spawn_action_t {
    public:

        uint64_t request_id;
        std::function<void(const std::error_code&)> handler;

        spawn_action_t(uint64_t rq_id, std::function<void(const std::error_code&)> handler_):
            request_id (rq_id),
            handler(handler_)
        {}
        
    } ;

    typedef result_of<io::conductor::subscribe>::type subscribe_result_t;
    typedef std::map<uint64_t, streamed<subscribe_result_t>> remote_map_t;

    typedef std::map<std::string, std::map<std::string, std::string>> request_map_t;

    typedef std::unique_ptr<request_map_t> request_ptr_t;
    
    typedef std::map<uint64_t, request_ptr_t> requests_map_t;

    typedef std::map<uint64_t, std::unique_ptr<spawn_action_t>> waiting_map_t;

    conductor_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args);

    auto
    on_subscribe(uint64_t client_id) -> streamed<subscribe_result_t>;

    void
    on_spool_done(uint64_t request_id, const std::error_code& ec, const std::string& error_message);

    void
    on_spawn_done(uint64_t request_id, const std::error_code& ec, const std::string& error_message);

    virtual
    auto
    prototype() const -> const io::basic_dispatch_t& {
        return *this;
    }

    void spool(std::string& image, std::function<void(const std::error_code&)> handler);

    void spawn(std::string image,
               std::string path,
               api::string_map_t&& isolate_params,
               api::string_map_t&& args,
               api::string_map_t&& environment,
               std::function<void(const std::error_code&)> handler);
    

private:
    std::shared_ptr<cocaine::logging::log_t> log;

    synchronized<remote_map_t> m_remotes;

    requests_map_t m_requests_pending;
    requests_map_t m_requests_waiting;

    waiting_map_t m_actions_waiting;

    uint64_t m_request_id;
    
};
}}
#endif
