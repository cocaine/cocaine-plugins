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

#include <cocaine/api/sandbox.hpp>

#include "readable_stream.hpp"

#include "python.hpp"

using namespace cocaine::sandbox;

request_emitter_t::request_emitter_t(python_t& sandbox):
    m_sandbox(sandbox)
{ }

void
request_emitter_t::push(const void * chunk,
                        size_t size)
{
    thread_lock_t lock(m_sandbox.thread_state()); 

    tracked_object_t buffer = PyString_FromStringAndSize(
        static_cast<const char*>(chunk),
        size
    );

    tracked_object_t args = PyTuple_Pack(1, *buffer);

    invoke("chunk", args, NULL);
}

void
request_emitter_t::close() {
    thread_lock_t lock(m_sandbox.thread_state()); 
    
    tracked_object_t args = PyTuple_New(0);
    
    invoke("close", args, NULL);
}

int
readable_stream_t::ctor(readable_stream_t * self,
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

    self->base = static_cast<request_emitter_t*>(
        PyCObject_AsVoidPtr(stream)
    );

    return 0;
}


void
readable_stream_t::dtor(readable_stream_t * self) {
    self->ob_type->tp_free(self);
}

PyObject*
readable_stream_t::on(readable_stream_t * self,
                      PyObject * args,
                      PyObject * kwargs)
{
    static char event_keyword[] = "event";
    static char callback_keyword[] = "callback";
    
    static char * keywords[] = {
        event_keyword,
        callback_keyword,
        NULL
    };

    const char * event = NULL;
    PyObject * callback = NULL;
    
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "sO:on", keywords, &event, &callback)) {
        return NULL;
    }

    if(!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "Callback is not a callable object");
    }

    self->base->on(event, callback);

    Py_RETURN_NONE;
}

static
PyMethodDef
readable_stream_object_methods[] = {
    { "on", (PyCFunction)readable_stream_t::on,
        METH_KEYWORDS, "Binds a callback method to the specified stream event" },
    { NULL }
};

PyTypeObject
readable_stream_object_type = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
    "cocaine.context.ReadableStream",           /* tp_name */
    sizeof(readable_stream_t),                  /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor)readable_stream_t::dtor,        /* tp_dealloc */
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
    "Readable Stream Object",                   /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    readable_stream_object_methods,             /* tp_methods */
    0,                                          /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc)readable_stream_t::ctor,          /* tp_init */
    0,                                          /* tp_alloc */
    PyType_GenericNew                           /* tp_new */
};

