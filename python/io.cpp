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

#include <cocaine/interfaces/sandbox.hpp>

#include "io.hpp"
#include "python.hpp"

using namespace cocaine::engine;

int python_io_t::constructor(python_io_t * self,
                             PyObject * args,
                             PyObject * kwargs)
{
    PyObject * io_object;

    if(!PyArg_ParseTuple(args, "O", &io_object)) {
        return -1;
    }

    self->io = static_cast<io_t*>(PyCObject_AsVoidPtr(io_object));

    return 0;
}


void python_io_t::destructor(python_io_t * self) {
    self->ob_type->tp_free(self);
    
    if(!self->request.empty()){
        static cocaine::blob_t dummy("d", 2);
        self->request = dummy;
        self->request.clear();
    }
}

PyObject* python_io_t::read(python_io_t * self,
                            PyObject * args,
                            PyObject * kwargs)
{
    static char size_keyword[] = "size";
    static char timeout_keyword[] = "timeout";
    static char * keywords[] = { size_keyword, timeout_keyword, NULL };

    Py_ssize_t size = 0;
    int timeout = 0;
    PyObject * result = NULL;

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "|ni:read", keywords, &size, &timeout)) {
        return result;
    }

    if(self->request.empty()) {
        Py_BEGIN_ALLOW_THREADS
            self->request = self->io->read(timeout);
        Py_END_ALLOW_THREADS
        
        self->offset = 0;
    }

    if(!self->request.empty() && (self->request.size() - self->offset)) {
        // NOTE: If the size argument is negative or omitted, read all data until EOF is reached.
        if(size <= 0 || size > (self->request.size() - self->offset)) {
            size = self->request.size() - self->offset;
        }

        result = PyBytes_FromStringAndSize(
            static_cast<const char *>(self->request.data()) + self->offset,
            size
        );

        self->offset += size;
    } else {
        result = PyBytes_FromString("");
    }

    return result;
}

PyObject* python_io_t::write(python_io_t * self,
                             PyObject * args)
{
    const char * message = NULL;

#ifdef  PY_SSIZE_T_CLEAN
    Py_ssize_t size = 0;
#else
    int size = 0;
#endif

    if(!PyArg_ParseTuple(args, "s#:write", &message, &size)) {
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
        if(message && size) {
            self->io->write(message, size);
        }
    Py_END_ALLOW_THREADS

    Py_RETURN_NONE;
}

/*
PyObject* python_io_t::delegate(python_io_t * self,
                                PyObject * args,
                                PyObject * kwargs)
{
    static char method_keyword[] = "method";
    static char message_keyword[] = "message";
    static char * keywords[] = { method_keyword, message_keyword, NULL };

    const char * target = NULL;
    const char * message = NULL;

#ifdef  PY_SSIZE_T_CLEAN
    Py_ssize_t size = 0;
#else
    int size = 0;
#endif

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "s|s#:delegate", keywords, &target, &message, &size)) {
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
        self->io->delegate(
            target,
            message,
            size
        );
    Py_END_ALLOW_THREADS

    Py_RETURN_NONE;
}
*/

PyObject* python_io_t::readline(python_io_t * self,
                                PyObject * args,
                                PyObject * kwargs)
{
    PyObject * result = NULL;

    if(self->request.empty()) {
        Py_BEGIN_ALLOW_THREADS
            self->request = self->io->read(-1);
        Py_END_ALLOW_THREADS
        
        self->offset = 0;
    }

    if(!self->request.empty() && (self->request.size() - self->offset)) {
        const char * data = static_cast<const char *>(self->request.data());
        off_t offset = self->offset;      

        while((self->request.size() - offset) && data[offset++]!='\n');

        result = PyBytes_FromStringAndSize(
            static_cast<const char *>(self->request.data()) + self->offset,
            offset-self->offset
        );

        self->offset += offset-self->offset;
    } else {
        result = PyBytes_FromString("");
    }    


    return result;
}

PyObject* python_io_t::readlines(python_io_t * self,
                                 PyObject * args,
                                 PyObject * kwargs)
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "Method is not yet implemented"
    );

    return NULL;
}

PyObject* python_io_t::iter_next(python_io_t * it) {
    PyErr_SetString(
        PyExc_NotImplementedError,
        "Method is not yet implemented"
    );

    return NULL;
}
