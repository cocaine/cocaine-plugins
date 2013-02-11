/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COCAINE_PYTHON_SANDBOX_HPP
#define COCAINE_PYTHON_SANDBOX_HPP

// NOTE: These are being redefined in Python.h
#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE

#include <Python.h>

#include "track.hpp"

#include <cocaine/api/sandbox.hpp>

namespace cocaine { namespace sandbox {

typedef track_t<
    PyObject*,
    Py_DecRef
> tracked_object_t;

struct thread_lock_t {
    thread_lock_t(PyThreadState * thread) {
        BOOST_ASSERT(thread != 0);
        PyEval_RestoreThread(thread);
    }

    ~thread_lock_t() {
        PyEval_SaveThread();
    }
};

PyObject*
wrap(const Json::Value& value);

std::string
exception();

struct event_source_t;

class python_t:
    public api::sandbox_t
{
    public:
        typedef api::sandbox_t category_type;

    public:
        python_t(context_t& context,
                 const std::string& name,
                 const Json::Value& args,
                 const std::string& spool);

        virtual
        ~python_t();

        virtual
        std::shared_ptr<api::stream_t>
        invoke(const std::string& method,
               const std::shared_ptr<api::stream_t>& upstream);

    public:
        logging::log_t*
        logger() {
            return m_log.get();
        }

        event_source_t*
        emitter() {
            return m_emitter.get();
        }

        PyThreadState*
        thread_state() {
            return m_thread_state;
        }

    private:
        context_t& m_context;
        std::unique_ptr<logging::log_t> m_log;

        // Main event source.
        std::unique_ptr<event_source_t> m_emitter;

        // Python state objects.
        PyObject * m_module;
        PyThreadState * m_thread_state;
};

}}

#endif
