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
#include "cocaine/api/v15/unicorn.hpp"

#include <cocaine/api/service.hpp>
#include <cocaine/rpc/dispatch.hpp>

namespace cocaine { namespace service {

/// Service for providing access to unified configuration service.
///
/// Currently we use Zookeeper as a backend. For protocol methods description see
/// `include/cocaine/idl/unicorn.hpp`.
class unicorn_service_t:
    public api::service_t,
    public dispatch<io::unicorn_tag>
{
    template<class Event, class Method, class Response>
    friend class unicorn_slot_t;

public:
    unicorn_service_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args);

    auto
    prototype() -> io::basic_dispatch_t& override {
        return *this;
    }

private:
    std::shared_ptr<logging::logger_t> log;

    std::shared_ptr<api::v15::unicorn_t> unicorn;
};

class unicorn_dispatch_t :
    public dispatch<io::unicorn_final_tag>
{
public:
    unicorn_dispatch_t(const std::string& name);

    auto
    attach(api::unicorn_scope_ptr scope) -> void;

    void
    discard(const std::error_code& ec) override;

private:
    // Because discard is marked as const.
    enum state_t {
        initial,
        discarded,
    } mutable state;

    mutable std::mutex mutex;
    mutable api::unicorn_scope_ptr scope;
};

}} //namespace cocaine::service
