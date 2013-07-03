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

#ifndef _TIMER_HPP_INCLUDED_
#define _TIMER_HPP_INCLUDED_

#include <cocaine/framework/service.hpp>
#include <cocaine/services/timer.hpp>
#include <cocaine/logging.hpp>

namespace cocaine { namespace framework {

typedef cocaine::io::protocol<cocaine::io::timer_tag>::version version_type;

class timer_service_t:
    public service_t
{
    public:
        timer_service_t(const std::string& name,
                             cocaine::io::reactor_t& service,
                             const cocaine::io::tcp::endpoint& resolver,
                             std::shared_ptr<logger_t> logger) :
            service_t(name, service, resolver, logger, version_type())
        { }

		service_t::handler<io::timer::notify_after>::future
        notify_after(double time, bool send_id = false) {
            return call<io::timer::notify_after>(time, send_id);
        }
			
        service_t::handler<io::timer::notify_every>::future
        notify_every(double time, bool send_id = false) {
            return call<io::timer::notify_every>(time, send_id);
        }
		
		service_t::handler<io::timer::cancel>::future
        cancel(io::timer_id_t timer_id) {
            return call<io::timer::cancel>(timer_id);
        }
		
		service_t::handler<io::timer::restart>::future
        restart(io::timer_id_t timer_id) {
            return call<io::timer::restart>(timer_id);
        }
};

}} // namespace cocaine::framework

#endif /* _TIMER_HPP_INCLUDED_ */
