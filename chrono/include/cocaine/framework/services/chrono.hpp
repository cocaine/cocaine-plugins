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

#ifndef _CHRONO_HPP_INCLUDED_
#define _CHRONO_HPP_INCLUDED_

#include <cocaine/framework/service.hpp>
#include <cocaine/services/chrono.hpp>

namespace cocaine { namespace framework {

class chrono_service_t:
    public service_t
{
    public:
        static const unsigned int version = cocaine::io::protocol<cocaine::io::chrono_tag>::version::value;

        chrono_service_t(std::shared_ptr<service_connection_t> connection) :
            service_t(connection)
        { }

        service_traits<cocaine::io::chrono::notify_after>::future_type
        notify_after(double time, bool send_id = false) {
            return call<io::chrono::notify_after>(time, send_id);
        }

        service_traits<cocaine::io::chrono::notify_every>::future_type
        notify_every(double time, bool send_id = false) {
            return call<io::chrono::notify_every>(time, send_id);
        }

        service_traits<cocaine::io::chrono::cancel>::future_type
        cancel(io::timer_id_t timer_id) {
            return call<io::chrono::cancel>(timer_id);
        }

        service_traits<cocaine::io::chrono::restart>::future_type
        restart(io::timer_id_t timer_id) {
            return call<io::chrono::restart>(timer_id);
        }
};

}} // namespace cocaine::framework

#endif /* _CHRONO_HPP_INCLUDED_ */
