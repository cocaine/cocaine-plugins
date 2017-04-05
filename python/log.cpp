/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
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

#include "log.hpp"

#include "sandbox.hpp"

#include <cocaine/logging.hpp>

using namespace cocaine::api;
using namespace cocaine::sandbox;

int
log_t::ctor(log_t * self,
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
    )->logger();

    return 0;
}

void
log_t::dtor(log_t * self) {
    self->ob_type->tp_free(self);
}

PyObject*
log_t::debug(log_t * self,
             PyObject * args)
{
    PyObject * object = NULL;
    const char * message = NULL;

    if(!PyArg_ParseTuple(args, "O:debug", &object)) {
        return NULL;
    }

    if(!PyString_Check(object)) {
        tracked_object_t string(PyObject_Str(object));
        message = PyString_AsString(string);
    } else {
        message = PyString_AsString(object);
    }

    COCAINE_LOG_DEBUG(self->base, "{}", message);

    Py_RETURN_NONE;
}

PyObject*
log_t::info(log_t * self,
            PyObject * args)
{
    PyObject * object = NULL;
    const char * message = NULL;

    if(!PyArg_ParseTuple(args, "O:info", &object)) {
        return NULL;
    }

    if(!PyString_Check(object)) {
        tracked_object_t string(PyObject_Str(object));
        message = PyString_AsString(string);
    } else {
        message = PyString_AsString(object);
    }

    COCAINE_LOG_INFO(self->base, "{}", message);

    Py_RETURN_NONE;
}

PyObject*
log_t::warning(log_t * self,
               PyObject * args)
{
    PyObject * object = NULL;
    const char * message = NULL;

    if(!PyArg_ParseTuple(args, "O:warning", &object)) {
        return NULL;
    }

    if(!PyString_Check(object)) {
        tracked_object_t string(PyObject_Str(object));
        message = PyString_AsString(string);
    } else {
        message = PyString_AsString(object);
    }

    COCAINE_LOG_WARNING(self->base, "{}", message);

    Py_RETURN_NONE;
}

PyObject*
log_t::error(log_t * self,
             PyObject * args)
{
    PyObject * object = NULL;
    const char * message = NULL;

    if(!PyArg_ParseTuple(args, "O:error", &object)) {
        return NULL;
    }

    if(!PyString_Check(object)) {
        tracked_object_t string(PyObject_Str(object));
        message = PyString_AsString(string);
    } else {
        message = PyString_AsString(object);
    }

    COCAINE_LOG_ERROR(self->base, "{}", message);

    Py_RETURN_NONE;
}

PyObject*
log_t::write(log_t * self,
             PyObject * args)
{
    return log_t::error(self, args);
}

PyObject*
log_t::writelines(log_t * self,
                  PyObject * args)
{
    PyObject * lines = NULL;

    if(!PyArg_ParseTuple(args, "O:writelines", &lines)) {
        return NULL;
    }

    tracked_object_t iterator(PyObject_GetIter(lines));
    tracked_object_t line(NULL);

    if(PyErr_Occurred()) {
        return NULL;
    }

    while(true) {
        line = PyIter_Next(iterator);

        if(!line.valid()) {
            if(!PyErr_Occurred()) {
                break;
            } else {
                return NULL;
            }
        }

        tracked_object_t argpack(PyTuple_Pack(1, *line));

        if(!write(self, argpack)) {
            return NULL;
        }
    }

    Py_RETURN_NONE;
}

PyObject*
log_t::flush(log_t * self,
             PyObject * args)
{
    Py_RETURN_NONE;
}

static
PyMethodDef
log_object_methods[] = {
    { "debug", (PyCFunction)log_t::debug, METH_VARARGS,
        "Logs a message with a Debug priority" },
    { "info", (PyCFunction)log_t::info, METH_VARARGS,
        "Logs a message with an Information priority" },
    { "warning", (PyCFunction)log_t::warning, METH_VARARGS,
        "Logs a message with a Warning priority" },
    { "error", (PyCFunction)log_t::error, METH_VARARGS,
        "Logs a message with an Error priority" },
    { "write", (PyCFunction)log_t::write, METH_VARARGS,
        "Writes a message to the error stream" },
    { "writelines", (PyCFunction)log_t::writelines, METH_VARARGS,
        "Writes messages from the iterable to the error stream" },
    { "flush", (PyCFunction)log_t::flush, METH_NOARGS,
        "Flushes the error stream" },
    { NULL, NULL, 0, NULL }
};

PyTypeObject
log_object_type = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
    "cocaine.context.Log",                      /* tp_name */
    sizeof(log_t),                              /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor)log_t::dtor,                    /* tp_dealloc */
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
    "Log Object",                               /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    log_object_methods,                         /* tp_methods */
    0,                                          /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc)log_t::ctor,                      /* tp_init */
    0,                                          /* tp_alloc */
    PyType_GenericNew                           /* tp_new */
};

