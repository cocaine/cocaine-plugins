/*
    Copyright (C) 2011-2013 Alexander Eliseev <admin@inkvi.com>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#include "dispatch.hpp"

#include "sandbox.hpp"

using namespace cocaine::sandbox;

int
dispatch_t::ctor(dispatch_t * self,
                 PyObject * args,
                 PyObject * kwargs)
{
    PyObject * builtins = PyEval_GetBuiltins();
    PyObject * sandbox = PyDict_GetItemString(builtins, "__sandbox__");

    if(!sandbox || !PyCObject_Check(sandbox)) {
        PyErr_SetString(PyExc_RuntimeError, "The context is corrupted");
        return -1;
    }

    self->base = static_cast<python_t*>(
        PyCObject_AsVoidPtr(sandbox)
    )->emitter();

    return 0;
}

void
dispatch_t::dtor(dispatch_t * self) {
    self->ob_type->tp_free(self);
}

PyObject*
dispatch_t::on(dispatch_t * self,
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
dispatch_object_methods[] = {
    { "on", (PyCFunction)dispatch_t::on,
        METH_KEYWORDS, "Binds a callback method to the specified event" },
    { NULL }
};

PyTypeObject
dispatch_object_type = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
    "cocaine.context.Dispatch",                 /* tp_name */
    sizeof(dispatch_t),                         /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor)dispatch_t::dtor,               /* tp_dealloc */
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
    "Event Dispatch Object",                    /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    dispatch_object_methods,                    /* tp_methods */
    0,                                          /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc)dispatch_t::ctor,                 /* tp_init */
    0,                                          /* tp_alloc */
    PyType_GenericNew                           /* tp_new */
};

