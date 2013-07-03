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

#include <cocaine/services/timer.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/traits/tuple.hpp>
#include <memory>

using namespace cocaine;
using namespace cocaine::io;
using namespace cocaine::io::timer;
using namespace cocaine::service;

using namespace std::placeholders;
using cocaine::service::timer_t;

timer_t::timer_t(context_t& context,
                       reactor_t& reactor,
                       const std::string& name,
                       const Json::Value& args):
    service_t(context, reactor, name, args),
    log_(new logging::log_t(context, name)),
	reactor_(reactor)
{
    on<io::timer::notify_after>("notify_after", std::bind(&timer_t::notify_after, this, _1, _2));
	on<io::timer::notify_every>("notify_every", std::bind(&timer_t::notify_every, this, _1, _2));
    on<io::timer::cancel>("cancel", std::bind(&timer_t::cancel, this, _1));
    on<io::timer::restart>("restart", std::bind(&timer_t::restart, this, _1));
}

cocaine::streamed<io::timer_id_t>
timer_t::notify_after(double time, bool send_id) {
    return set_timer_impl(time, 0.0, send_id);
}

cocaine::streamed<io::timer_id_t>
timer_t::notify_every(double time, bool send_id) {
	return set_timer_impl(time, time, send_id);
}

cocaine::streamed<io::timer_id_t> 
timer_t::set_timer_impl(double first, double repeat, bool send_id) {
	std::shared_ptr<streamed<io::timer_id_t>> promise(new streamed<io::timer_id_t>());
	std::shared_ptr<ev::timer> timer(new ev::timer(reactor_.native()));
    
    timer_desc_t desc;
    desc.timer_ = timer;
	desc.promise_ = promise;
    
	io::timer_id_t timer_id;
	do {
		timer_id = std::rand();
	} while (timers_.find(timer_id) != timers_.end());
	
	try {
		timers_.insert( std::make_pair(timer_id, desc) );
		timer_to_id_[timer.get()] = timer_id;

		timer->set<timer_t, &timer_t::on_timer>(this);
		timer->start(first, repeat);
		
		if (send_id) {
			promise->write(timer_id);
		}
	} catch (std::exception& ex) {
		remove_timer(timer_id);
		throw;
	}
	
	return *promise;
}

void
timer_t::cancel(io::timer_id_t timer_id) {
    if (timers_.find(timer_id) != timers_.end()) {
		remove_timer(timer_id);
	} else {
		COCAINE_LOG_INFO(log_, "Attempt to cancel timer that does not exist, id is %1%", timer_id);
	}
}

void
timer_t::restart(io::timer_id_t timer_id) {
    const timer_desc_t& timer_desc = timers_.at(timer_id);
	timer_desc.timer_->again();
}

void 
timer_t::on_timer(ev::timer &timer, int revents) {
	try {
        io::timer_id_t timer_id = timer_to_id_.at(&timer);
        
		const timer_desc_t& timer_desc = timers_.at(timer_id);

		try {
			timer_desc.promise_->write(timer_id);
		} catch (std::exception ex) { 
			remove_timer(timer_id);
		}
		
		if (!timer.is_active() && timers_.find(timer_id) != timers_.end()) {
            remove_timer(timer_id);
        }
    } catch (std::exception& ex) {
		COCAINE_LOG_ERROR(log_, "Possibly bug in timer service in timer_t::on_timer, error is %s", ex.what());
    }
}

void
timer_t::remove_timer(io::timer_id_t timer_id) {
	try {
		timers_.at(timer_id).promise_->close();
		ev::timer* timer_ptr = timers_.at(timer_id).timer_.get();
		timers_.erase(timer_id);
		timer_to_id_.erase(timer_ptr);
	} catch (std::exception& ex) {
		COCAINE_LOG_ERROR(log_, "Error occured while removing timer %s", ex.what());
	}
}
