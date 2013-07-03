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

#ifndef COCAINE_TIMER_SERVICE_HPP
#define COCAINE_TIMER_SERVICE_HPP

#include <cocaine/api/service.hpp>
#include <cocaine/asio/reactor.hpp>
#include <cocaine/rpc/tags.hpp>
#include <cocaine/rpc/slots/streamed.hpp>

namespace cocaine { namespace io {

struct timer_tag;

typedef int64_t timer_id_t;

namespace timer {
    struct notify_after {
        typedef timer_tag tag;

        typedef boost::mpl::list<
            /* time difference */ double,
			/* send id */ optional_with_default<bool, false>
        > tuple_type;

        typedef timer_id_t result_type;
    };
	
	struct notify_every {
        typedef timer_tag tag;

        typedef boost::mpl::list<
            /* time difference */ double,
			/* send id */ optional_with_default<bool, false>
        > tuple_type;

        typedef timer_id_t result_type;
    };

    struct cancel {
        typedef timer_tag tag;

        typedef boost::mpl::list<
            /* timer id */ timer_id_t
        > tuple_type;

        typedef void result_type;
    };
    
    struct restart {
        typedef timer_tag tag;

        typedef boost::mpl::list<
            /* timer id */ timer_id_t
        > tuple_type;

        typedef void result_type;
    };
}

template<>
struct protocol<timer_tag> {
    typedef mpl::list<
        timer::notify_after,
		timer::notify_every,
        timer::cancel,
        timer::restart
    > type;

    typedef boost::mpl::int_<
        1
    >::type version;
};

} // namespace io

namespace detail {
    template<class R>
    struct select<streamed<R>> {
        template<class Sequence>
        struct apply {
            typedef io::streamed_slot<streamed<R>, Sequence> type;
        };
    };
}

namespace service {

class timer_t:
    public api::service_t
{
    public:
        timer_t(context_t& context,
                   io::reactor_t& reactor,
                   const std::string& name,
                   const Json::Value& args);

    private:
        struct timer_desc_t {
            std::shared_ptr<ev::timer> timer_;
			std::shared_ptr<streamed<io::timer_id_t>> promise_;
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
        on_timer(ev::timer &w, int revents);

        void
        remove_timer(io::timer_id_t timer_id);
        
    private:
		streamed<io::timer_id_t>
		set_timer_impl(double first, double repeat, bool send_id);
		
        std::shared_ptr<logging::log_t> log_;
        std::map<io::timer_id_t, timer_desc_t > timers_;
        std::map<ev::timer*, io::timer_id_t> timer_to_id_;
		ev::timer update_timer_;
		cocaine::io::reactor_t& reactor_;
};

}} // namespace cocaine::service

#endif
