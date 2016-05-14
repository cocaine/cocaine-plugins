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

#include "sandbox.hpp"

#include "log.hpp"
#include "readable_stream.hpp"
#include "writable_stream.hpp"
#include "dispatch.hpp"

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

#include <sstream>

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>

using namespace cocaine;
using namespace cocaine::sandbox;

PyObject*
cocaine::sandbox::wrap(const Json::Value& value) {
    PyObject * object = NULL;

    switch(value.type()) {
    case Json::booleanValue: {
        return PyBool_FromLong(value.asBool());
    }

    case Json::intValue:
    case Json::uintValue: {
        return PyLong_FromLong(value.asInt());
    }

    case Json::realValue: {
        return PyFloat_FromDouble(value.asDouble());
    }

    case Json::stringValue: {
        return PyString_FromString(value.asCString());
    }

    case Json::objectValue: {
        object = PyDict_New();
        Json::Value::Members names(value.getMemberNames());

        for(Json::Value::Members::iterator it = names.begin();
            it != names.end();
            ++it)
        {
            PyDict_SetItemString(object, it->c_str(), wrap(value[*it]));
        }
    } break;

    case Json::arrayValue: {
        object = PyTuple_New(value.size());
        Py_ssize_t position = 0;

        for(Json::Value::const_iterator it = value.begin();
            it != value.end();
            ++it)
        {
            PyTuple_SetItem(object, position++, wrap(*it));
        }

    } break;

    case Json::nullValue: {
        Py_RETURN_NONE;
    }}

    return object;
}

std::string
cocaine::sandbox::exception() {
    tracked_object_t type(NULL), value(NULL), traceback(NULL);

    PyErr_Fetch(&type, &value, &traceback);

    tracked_object_t name(PyObject_Str(type));
    tracked_object_t message(PyObject_Str(value));

    return cocaine::format(
        "uncaught exception {}: {}",
        PyString_AsString(name),
        PyString_AsString(message)
    );
}

python_t::python_t(context_t& context,
                   const std::string& name,
                   const Json::Value& args,
                   const std::string& spool):
    category_type(context, name, args, spool),
    m_context(context),
    m_log(new logging::log_t(context, cocaine::format("app/{}", name))),
    m_emitter(new event_source_t()),
    m_module(NULL),
    m_thread_state(NULL)
{
    Py_InitializeEx(0);
    PyEval_InitThreads();

    // Initializing types

    PyType_Ready(&log_object_type);
    PyType_Ready(&readable_stream_object_type);
    PyType_Ready(&writable_stream_object_type);
    PyType_Ready(&dispatch_object_type);

    boost::filesystem::path source(spool);

    // NOTE: Means it's a module.
    if(boost::filesystem::is_directory(source)) {
        source /= "__init__.py";
    }

    COCAINE_LOG_DEBUG(m_log, "loading the app code from {}", source.string());

    boost::filesystem::ifstream input(source);

    if(!input) {
        throw cocaine::error_t("unable to open '{}'", source.string());
    }

    // System paths

    std::unique_ptr<char, void(*)(void*)> key(::strdup("path"), &::free);

    // NOTE: Prepend the current app location to the sys.path,
    // so that it could import various local stuff from there.
    PyObject * syspaths = PySys_GetObject(key.get());

    tracked_object_t path(
        PyString_FromString(
#if BOOST_FILESYSTEM_VERSION == 3
            source.parent_path().string().c_str()
#else
            source.branch_path().string().c_str()
#endif
        )
    );

    PyList_Insert(syspaths, 0, path);

    // Context access module

    PyObject * context_module = Py_InitModule(
        "__context__",
        NULL
    );

    Py_INCREF(&log_object_type);

    PyModule_AddObject(
        context_module,
        "Log",
        reinterpret_cast<PyObject*>(&log_object_type)
    );

    Py_INCREF(&readable_stream_object_type);

    PyModule_AddObject(
        context_module,
        "ReadableStream",
        reinterpret_cast<PyObject*>(&readable_stream_object_type)
    );

    Py_INCREF(&writable_stream_object_type);

    PyModule_AddObject(
        context_module,
        "WritableStream",
        reinterpret_cast<PyObject*>(&writable_stream_object_type)
    );

    Py_INCREF(&dispatch_object_type);

    PyModule_AddObject(
        context_module,
        "Dispatch",
        reinterpret_cast<PyObject*>(&dispatch_object_type)
    );

    PyModule_AddObject(
        context_module,
        "args",
        PyDictProxy_New(wrap(args))
    );

    // App module

    m_module = Py_InitModule(
        name.c_str(),
        NULL
    );

    PyObject * builtins = PyEval_GetBuiltins();

    PyDict_SetItemString(
        builtins,
        "__sandbox__",
        PyCObject_FromVoidPtr(this, NULL)
    );

    Py_INCREF(builtins);

    PyModule_AddObject(
        m_module,
        "__builtins__",
        builtins
    );

    PyModule_AddStringConstant(
        m_module,
        "__file__",
        source.string().c_str()
    );

    // Code evaluation

    std::stringstream stream;
    stream << input.rdbuf();

    tracked_object_t bytecode(
        Py_CompileString(
            stream.str().c_str(),
            source.string().c_str(),
            Py_file_input
        )
    );

    if(PyErr_Occurred()) {
        throw cocaine::error_t("{}", exception());
    }

    PyObject * globals = PyModule_GetDict(m_module);

    // NOTE: This will return None or NULL due to the Py_file_input flag above,
    // so we can safely drop it without even checking.
    tracked_object_t result(
        PyEval_EvalCode(
            reinterpret_cast<PyCodeObject*>(*bytecode),
            globals,
            NULL
        )
    );

    if(PyErr_Occurred()) {
        throw cocaine::error_t("{}", exception());
    }

    m_thread_state = PyEval_SaveThread();
}

python_t::~python_t() {
    if(m_thread_state) {
        PyEval_RestoreThread(m_thread_state);
    }

    Py_Finalize();
}

std::shared_ptr<api::stream_t>
python_t::invoke(const std::string& event,
                 const std::shared_ptr<api::stream_t>& upstream)
{
    thread_lock_t thread(m_thread_state);

    std::shared_ptr<api::stream_t> downstream(
        std::make_shared<downstream_t>(*this)
    );

    // Pack the arguments

    tracked_object_t args(NULL);

    tracked_object_t downstream_ptr_object(
        PyCObject_FromVoidPtr(downstream.get(), NULL)
    );

    args = PyTuple_Pack(1, *downstream_ptr_object);

    tracked_object_t downstream_object(
        PyObject_Call(
            reinterpret_cast<PyObject*>(&readable_stream_object_type),
            args,
            NULL
        )
    );

    tracked_object_t upstream_ptr_object(
        PyCObject_FromVoidPtr(upstream.get(), NULL)
    );

    args = PyTuple_Pack(1, *upstream_ptr_object);

    tracked_object_t upstream_object(
        PyObject_Call(
            reinterpret_cast<PyObject*>(&writable_stream_object_type),
            args,
            NULL
        )
    );

    args = PyTuple_Pack(2, *downstream_object, *upstream_object);

    // Call the event handler

    if(!m_emitter->invoke(event, args, NULL)) {
        throw cocaine::error_t("the '{}' event is not supported", event);
    }

    if(PyErr_Occurred()) {
        throw cocaine::error_t("{}", exception());
    }

    return downstream;
}

