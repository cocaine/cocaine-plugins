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

#include "writable_stream.hpp"

#include <cocaine/api/stream.hpp>

using namespace cocaine::sandbox;

int
writable_stream_t::ctor(writable_stream_t * self,
                        PyObject * args,
                        PyObject * kwargs)
{
    PyObject * stream;

    if(!PyArg_ParseTuple(args, "O", &stream)) {
        return -1;
    }

    if(!stream || !PyCObject_Check(stream)) {
        PyErr_SetString(PyExc_RuntimeError, "The context is corrupted");
        return -1;
    }

    self->base = static_cast<api::stream_t*>(
        PyCObject_AsVoidPtr(stream)
    );

    return 0;
}


void
writable_stream_t::dtor(writable_stream_t * self) {
    self->ob_type->tp_free(self);
}

PyObject*
writable_stream_t::write(writable_stream_t * self,
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
        try {
            self->base->write(message, size);
        } catch(cocaine::error_t& e) {
            // Do nothing.
        }
    Py_END_ALLOW_THREADS

    Py_RETURN_NONE;
}

PyObject*
writable_stream_t::close(writable_stream_t * self,
                         PyObject * args)
{
    Py_BEGIN_ALLOW_THREADS
        try {
            self->base->close();
        } catch(cocaine::error_t& e) {
            // Do nothing.
        }
    Py_END_ALLOW_THREADS

    Py_RETURN_NONE;
}

static
PyMethodDef
writable_stream_object_methods[] = {
    { "write", (PyCFunction)writable_stream_t::write,
        METH_VARARGS, "Writes a new chunk of data into the stream" },
    { "close", (PyCFunction)writable_stream_t::close,
        METH_VARARGS, "Closes the stream" },
    { NULL }
};

PyTypeObject
writable_stream_object_type = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
    "cocaine.context.WritableStream",           /* tp_name */
    sizeof(writable_stream_t),                  /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor)writable_stream_t::dtor,        /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_compare */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                         /* tp_flags */
    "Writable Stream Object",                   /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    writable_stream_object_methods,             /* tp_methods */
    0,                                          /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc)writable_stream_t::ctor,          /* tp_init */
    0,                                          /* tp_alloc */
    PyType_GenericNew                           /* tp_new */
};

