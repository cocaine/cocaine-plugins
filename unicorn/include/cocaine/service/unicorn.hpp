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

#pragma once

#include "cocaine/idl/unicorn.hpp"

#include <cocaine/api/unicorn.hpp>
#include <cocaine/api/service.hpp>
#include <cocaine/rpc/dispatch.hpp>

namespace cocaine { namespace service {

class unicorn_service_t:
    public api::service_t,
    public dispatch<io::unicorn_tag>
{
public:
    virtual
    const io::basic_dispatch_t&
    prototype() const;

    unicorn_service_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args);
    friend class unicorn_dispatch_t;
    friend class distributed_lock_t;

    template<class Event, class Method, class Response>
    friend class unicorn_slot_t;

    const std::string& get_name() const { return name; }
private:
    std::string name;
    std::shared_ptr<api::unicorn_t> unicorn;
    std::shared_ptr<logging::logger_t> log;
};

/**
* Service for providing access to unified configuration service.
* Currently we use zookeeper as backend.
* For protocol methods description see include/cocaine/idl/unicorn.hpp
*/
class unicorn_dispatch_t :
    public dispatch<io::unicorn_final_tag>
{
public:
    unicorn_dispatch_t(const std::string& name, unicorn_service_t* service, api::unicorn_scope_ptr scope);

    virtual
    void discard(const std::error_code& ec) const;

private:
    // because discard is marked const
    mutable api::unicorn_scope_ptr scope;
};

}} //namespace cocaine::service
