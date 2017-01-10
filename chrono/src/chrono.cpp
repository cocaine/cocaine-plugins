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

#include "cocaine/chrono.hpp"

#include <memory>

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

#include <cocaine/traits/tuple.hpp>

using namespace cocaine;
using namespace cocaine::io;
using namespace cocaine::service;

namespace ph = std::placeholders;

chrono_t::chrono_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args):
    service_t(context, asio, name, args),
    dispatch<io::chrono_tag>(name),
    log_(context.log(name)),
    asio_(asio)
{
    on<io::chrono::notify_after>(std::bind(&chrono_t::notify_after, this, ph::_1, ph::_2));
    on<io::chrono::notify_every>(std::bind(&chrono_t::notify_every, this, ph::_1, ph::_2));
    on<io::chrono::cancel>(std::bind(&chrono_t::cancel, this, ph::_1));
    on<io::chrono::restart>(std::bind(&chrono_t::restart, this, ph::_1));
}

cocaine::streamed<io::timer_id_t>
chrono_t::notify_after(double time, bool send_id) {
    return set_timer_impl(time, 0.0, send_id);
}

cocaine::streamed<io::timer_id_t>
chrono_t::notify_every(double time, bool send_id) {
    return set_timer_impl(time, time, send_id);
}

cocaine::streamed<io::timer_id_t>
chrono_t::set_timer_impl(double first, double repeat, bool send_id) {
    streamed<io::timer_id_t> promise;
    std::shared_ptr<asio::deadline_timer> timer(new asio::deadline_timer(asio_));

    timer_desc_t desc;
    desc.timer_ = timer;
    desc.interval_ = repeat;

    io::timer_id_t timer_id;

    auto ptr = timers_.synchronize();

    do {
        timer_id = std::rand();
    } while (ptr->find(timer_id) != ptr->end());

    try {
        ptr->insert(std::make_pair(timer_id, desc));

        desc.timer_->expires_from_now(boost::posix_time::seconds(first));
        desc.timer_->async_wait(std::bind(&chrono_t::on_timer, this, ph::_1, timer_id));

        if (send_id) {
            desc.promise_.write(timer_id);
        }
    } catch (std::exception& ex) {
        remove_timer(timer_id);
        throw;
    }

    return desc.promise_;
}

void
chrono_t::cancel(io::timer_id_t timer_id) {
    auto ptr = timers_.synchronize();

    if (ptr->find(timer_id) != ptr->end()) {
        remove_timer(timer_id);
    } else {
        COCAINE_LOG_INFO(log_, "attempt to cancel timer that does not exist, id is %1%", timer_id);
    }
}

void
chrono_t::restart(io::timer_id_t timer_id) {
    timer_desc_t& timer_desc = timers_.unsafe().at(timer_id);

    timer_desc.timer_->expires_from_now(boost::posix_time::seconds(timer_desc.interval_));
    timer_desc.timer_->async_wait(std::bind(&chrono_t::on_timer, this, ph::_1, timer_id));
}

void
chrono_t::on_timer(const std::error_code& ec, io::timer_id_t timer_id) {
    if(ec == asio::error::operation_aborted) {
        return;
    }

    BOOST_ASSERT(!ec);

    auto ptr = timers_.synchronize();

    try {
        timer_desc_t timer_desc = ptr->at(timer_id);

        try {
            timer_desc.promise_.write(timer_id);
        } catch (const std::exception& ex) {
            remove_timer(timer_id);
        }

        if (!timer_desc.interval_ && ptr->find(timer_id) != ptr->end()) {
            remove_timer(timer_id);
        } else if (timer_desc.interval_ && ptr->find(timer_id) != ptr->end()) {
            restart(timer_id);
        }
    } catch (std::exception& ex) {
        COCAINE_LOG_ERROR(log_, "possibly bug in timer service in chrono_t::on_timer(), error is %s", ex.what());
    }
}

void
chrono_t::remove_timer(io::timer_id_t timer_id) {
    try {
        // Already locked.
        timers_.unsafe().at(timer_id).promise_.close();
    } catch (const std::exception& ex) {
        COCAINE_LOG_ERROR(log_, "error occured while removing timer %s", ex.what());
    }
    timers_.unsafe().erase(timer_id);
}
