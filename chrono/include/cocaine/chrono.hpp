/*
* 2013+ Copyright (c) Alexander Ponomarev <noname@yandex-team.ru>
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

#ifndef COCAINE_CHRONO_SERVICE_HPP
#define COCAINE_CHRONO_SERVICE_HPP

#include <cocaine/api/service.hpp>

#include "cocaine/idl/chrono.hpp"
#include <cocaine/rpc/dispatch.hpp>

#include <cocaine/locked_ptr.hpp>

#include <asio/deadline_timer.hpp>

namespace cocaine { namespace service {

class chrono_t:
    public api::service_t,
    public dispatch<io::chrono_tag>
{
    public:
        chrono_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args);

        virtual
        auto
        prototype() -> io::basic_dispatch_t& {
            return *this;
        }

    private:
        struct timer_desc_t {
            std::shared_ptr<asio::deadline_timer> timer_;
            streamed<io::timer_id_t> promise_;
            double interval_;
        };

        streamed<io::timer_id_t>
        notify_after(double time, bool send_id);

        streamed<io::timer_id_t>
        notify_every(double time, bool send_id);

        void
        cancel(io::timer_id_t timer_id);

        void
        restart(io::timer_id_t timer_id);

        void
        on_timer(const std::error_code& ec, io::timer_id_t timer_id);

        void
        remove_timer(io::timer_id_t timer_id);

    private:
        streamed<io::timer_id_t>
        set_timer_impl(double first, double repeat, bool send_id);

        std::unique_ptr<logging::logger_t> log_;
        synchronized<std::map<io::timer_id_t, timer_desc_t>> timers_;
        asio::io_service& asio_;
};

}} // namespace cocaine::service

#endif
