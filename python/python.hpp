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

#include <cocaine/api/sandbox.hpp>

#include "track.hpp"

namespace cocaine { namespace sandbox {

typedef track_t<
    PyObject*,
    Py_DecRef
> tracked_object_t;

class thread_lock_t {
    public:
        thread_lock_t(PyThreadState * thread) {
            BOOST_ASSERT(thread != 0);
            PyEval_RestoreThread(thread);
        }

        ~thread_lock_t() {
            PyEval_SaveThread();
        }
};

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
        void
        invoke(const std::string& method,
               api::io_t& io);

    public:
        const logging::logger_t&
        log() const {
            return *m_log;
        }
    
    public:
        static
        PyObject*
        args(PyObject * self,
             PyObject * args);
        
        static
        PyObject*
        wrap(const Json::Value& value);
        
        static
        std::string
        exception();

    private:
        boost::shared_ptr<logging::logger_t> m_log;
        
        PyObject * m_module;
        tracked_object_t m_manifest;
        
        PyThreadState * m_thread_state;
};

}}

#endif
