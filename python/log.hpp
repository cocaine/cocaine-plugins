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

#ifndef COCAINE_PYTHON_SANDBOX_LOG_HPP
#define COCAINE_PYTHON_SANDBOX_LOG_HPP

// NOTE: These are being redefined in Python.h
#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE

#include "Python.h"

#include <cocaine/common.hpp>

namespace cocaine { namespace sandbox {

struct log_t {
    PyObject_HEAD

    static
    int
    ctor(log_t * self,
         PyObject * args,
         PyObject * kwargs);

    static
    void
    dtor(log_t * self);

    static
    PyObject*
    debug(log_t * self,
          PyObject * args);

    static
    PyObject*
    info(log_t * self,
         PyObject * args);

    static
    PyObject*
    warning(log_t * self,
            PyObject * args);

    static
    PyObject*
    error(log_t * self,
          PyObject * args);

    // WSGI requirements.
    
    static
    PyObject*
    write(log_t * self,
          PyObject * args);

    static
    PyObject*
    writelines(log_t * self,
               PyObject * args);

    static
    PyObject*
    flush(log_t * self,
          PyObject * args);

public:
    const logging::logger_t * base;
};

}}

extern PyTypeObject log_object_type;

#endif
