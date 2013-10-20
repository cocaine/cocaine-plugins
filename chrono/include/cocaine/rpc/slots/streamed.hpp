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

#ifndef COCAINE_IO_STREAMED_SLOT_HPP
#define COCAINE_IO_STREAMED_SLOT_HPP

#include "cocaine/rpc/slots/function.hpp"

#include <mutex>
#include <iostream>

namespace cocaine { namespace io {

// Streamed slot

template<class R, class Event, class Sequence = typename event_traits<Event>::tuple_type>
struct streamed_slot:
    public function_slot<R, Sequence>
{
    typedef function_slot<R, Sequence> parent_type;
    typedef typename parent_type::callable_type callable_type;

    streamed_slot(callable_type callable):
        parent_type(Event::alias(), callable)
    { }

    virtual
    std::shared_ptr<dispatch_t>
    operator()(const msgpack::object& unpacked, const api::stream_ptr_t& upstream) {
        this->call(unpacked).attach(upstream);

        // Return an empty protocol dispatch.
        return std::shared_ptr<dispatch_t>();
    }
};

} // namespace io

namespace detail {
    struct streamed_state_t {
        streamed_state_t():
            m_packer(m_buffer),
            m_completed(false),
            m_failed(false)
        { }

        template<class T>
        void
        write(const T& value) {
            std::lock_guard<std::mutex> guard(m_mutex);

            if(m_completed) {
                return;
            }

            io::type_traits<T>::pack(m_packer, value);

            if(m_upstream) {
                m_upstream->write(m_buffer.data(), m_buffer.size());
                m_buffer.clear();
            }
        }

        void
        abort(int code, const std::string& reason) {
            std::lock_guard<std::mutex> guard(m_mutex);

            if(m_completed) {
                return;
            }

            m_code = code;
            m_reason = reason;

            if(m_upstream) {
                m_upstream->error(m_code, m_reason);
                m_upstream->close();
            }

            m_failed = true;
        }

        void
        close() {
            std::lock_guard<std::mutex> guard(m_mutex);

            if(m_completed) {
                return;
            }

            if(m_upstream) {
                m_upstream->close();
            }

            m_completed = true;
        }

        void
        attach(const api::stream_ptr_t& upstream) {
            std::lock_guard<std::mutex> guard(m_mutex);

            m_upstream = upstream;

            if (!m_failed) {
                if (m_buffer.size() > 0) {
                    m_upstream->write(m_buffer.data(), m_buffer.size());
                    m_buffer.clear();
                }
            } else {
                m_upstream->error(m_code, m_reason);
                m_upstream->close();
            }
        }

    private:
        msgpack::sbuffer m_buffer;
        msgpack::packer<msgpack::sbuffer> m_packer;

        int m_code;
        std::string m_reason;

        bool m_completed,
             m_failed;

        api::stream_ptr_t m_upstream;
        std::mutex m_mutex;
    };
}

template<class T>
struct streamed {
    streamed():
        m_state(new detail::streamed_state_t())
    { }

    void
    attach(const api::stream_ptr_t& upstream) {
        m_state->attach(upstream);
    }

    void
    write(const T& value) {
        m_state->write(value);
    }

    void
    close() {
        m_state->close();
    }

    void
    abort(int code, const std::string& reason) {
        m_state->abort(code, reason);
    }

private:
    const std::shared_ptr<detail::streamed_state_t> m_state;
};

template<>
struct streamed<void> {
    streamed():
        m_state(new detail::streamed_state_t())
    { }

    void
    attach(const api::stream_ptr_t& upstream) {
        m_state->attach(upstream);
    }

    void
    close() {
        m_state->close();
    }

    void
    abort(int code, const std::string& reason) {
        m_state->abort(code, reason);
    }

private:
    const std::shared_ptr<detail::streamed_state_t> m_state;
};

} // namespace cocaine

#endif
