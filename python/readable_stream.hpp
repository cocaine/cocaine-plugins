/*
    Copyright (C) 2011-2012 Alexander Eliseev <admin@inkvi.com>
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

#ifndef COCAINE_PYTHON_SANDBOX_READABLE_STREAM_HPP
#define COCAINE_PYTHON_SANDBOX_READABLE_STREAM_HPP

// NOTE: These are being redefined in Python.h
#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE

#include "Python.h"

#include <cocaine/common.hpp>

#include "event_source.hpp"

namespace cocaine {

namespace api {
    struct stream_t;
}

namespace sandbox {

class python_t;

struct request_stream_t:
    public api::stream_t,
    public event_source_t
{
    request_stream_t(python_t& sandbox);

    virtual
    void
    push(const void * chunk,
         size_t size);

    virtual
    void
    close();

    virtual
    void
    abort(error_code code,
          const std::string& message);

private:
    python_t& m_sandbox;
};

struct readable_stream_t {
    PyObject_HEAD

    static
    int
    ctor(readable_stream_t * self,
         PyObject * args,
         PyObject * kwargs);

    static
    void
    dtor(readable_stream_t * self);

    // Event binding

    static
    PyObject*
    on(readable_stream_t * self,
       PyObject * args,
       PyObject * kwargs);

public:
    request_stream_t * base;
};

}}

extern PyTypeObject readable_stream_object_type;

#endif
