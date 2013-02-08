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

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/format.hpp>

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/manifest.hpp>

#include <sstream>

#include "python.hpp"
#include "objects.hpp"

using namespace cocaine;
using namespace cocaine::engine;

namespace fs = boost::filesystem;

static PyMethodDef context_module_methods[] = {
    { "manifest", &python_t::manifest, METH_NOARGS, 
        "Get the application's manifest" },
    { NULL, NULL, 0, NULL }
};

static char python_home[255];

python_t::python_t(context_t& context, const manifest_t& manifest):
    category_type(context, manifest),
    m_log(context.log(
        (boost::format("app/%1%")
            % manifest.name
        ).str()
    )),
    m_python_module(NULL),
    m_python_manifest(NULL),
    m_thread_state(NULL)
{
    Json::Value args(manifest.root["args"]);

    if(!args.isObject()) {
        throw configuration_error_t("malformed manifest");
    }
    
    fs::path source(manifest.path);
   
    if(args["bundle"].asBool()) {
        std::memset(python_home, 0, sizeof(python_home));
        std::memcpy(python_home, source.string().c_str(), source.string().size());
        Py_SetPythonHome(python_home);
    }

    Py_InitializeEx(0);
    PyEval_InitThreads();

    PyType_Ready(&log_object_type);
    PyType_Ready(&python_io_object_type);
    
    // Context access module

    m_python_manifest = wrap(args);

    PyObject * context_module = Py_InitModule(
        "__context__",
        context_module_methods
    );

    Py_INCREF(&log_object_type);

    PyModule_AddObject(
        context_module,
        "Log",
        reinterpret_cast<PyObject*>(&log_object_type)
    );

    // Sandbox access object

    PyObject * builtins_module = PyEval_GetBuiltins();

    tracked_object_t sandbox(
        PyCObject_FromVoidPtr(this, NULL)
    );

    PyDict_SetItemString(
        builtins_module,
        "__sandbox__",
        sandbox
    );
    
    if(args["bundle"].asBool()) {
        load_bundle(manifest);
    } else {
        load_module(manifest);
    }

    m_thread_state = PyEval_SaveThread();
}

void
python_t::load_bundle(const manifest_t& manifest) {
    const std::string module = manifest.root["args"]["module"].asString();
   
    if(module.empty()) {
        throw configuration_error_t("the bundle module is not specified");
    }

    log().debug(
        "loading the app bundle from the '%s' module",
        module.c_str()
    );
    
    // Try to import the specified module.
    m_python_module = PyImport_ImportModule(module.c_str());

    if(PyErr_Occurred()) {
        throw unrecoverable_error_t(exception());
    }
}

void
python_t::load_module(const manifest_t& manifest) {
    fs::path source = fs::path(manifest.path) / "__init__.py";
    
    // System paths

    static char key[] = "path";

    // NOTE: Prepend the current app location to the sys.path,
    // so that it could import various local stuff from there.
    PyObject * syspaths = PySys_GetObject(key);
    
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
   
    // Code evaluation

    log().debug("loading the app code from %s", source.string().c_str());
    
    boost::filesystem::ifstream input(source);
    
    if(!input) {
        throw configuration_error_t("unable to open " + source.string());
    }

    m_python_module = Py_InitModule(
        manifest.name.c_str(),
        NULL
    );

    PyObject * builtins = PyEval_GetBuiltins();

    Py_INCREF(builtins);
    
    PyModule_AddObject(
        m_python_module, 
        "__builtins__",
        builtins
    );
    
    PyModule_AddStringConstant(
        m_python_module,
        "__file__",
        source.string().c_str()
    );

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
        throw unrecoverable_error_t(exception());
    }

    PyObject * globals = PyModule_GetDict(m_python_module);
    
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
        throw unrecoverable_error_t(exception());
    }
}

python_t::~python_t() {
    if(m_thread_state) {
        PyEval_RestoreThread(m_thread_state); 
    }
        
    Py_Finalize();
}

void python_t::invoke(const std::string& event,
                      io_t& io)
{
    thread_lock_t thread(m_thread_state);

    if(!m_python_module) {
        throw unrecoverable_error_t("python module is not initialized");
    }

    PyObject * globals = PyModule_GetDict(m_python_module);
    PyObject * object = PyDict_GetItemString(globals, event.c_str());
    
    if(!object) {
        throw unrecoverable_error_t("callable '" + event + "' does not exist");
    }
    
    if(PyType_Check(object)) {
        if(PyType_Ready(reinterpret_cast<PyTypeObject*>(object)) != 0) {
            throw unrecoverable_error_t(exception());
        }
    }

    if(!PyCallable_Check(object)) {
        throw unrecoverable_error_t("'" + event + "' is not callable");
    }

    tracked_object_t args(NULL);

    // Passing io_t object to the python io_t wrapper.
    tracked_object_t io_object(
        PyCObject_FromVoidPtr(&io, NULL)
    );

    args = PyTuple_Pack(1, *io_object);

    tracked_object_t io_proxy(
        PyObject_Call(
            reinterpret_cast<PyObject*>(&python_io_object_type), 
            args,
            NULL
        )
    );

    args = PyTuple_Pack(1, *io_proxy);

    tracked_object_t result(PyObject_Call(object, args, NULL));

    if(PyErr_Occurred()) {
        throw recoverable_error_t(exception());
    } 
   
    if(result != Py_None) {
        log().warning(
            "ignoring an unused returned value of callable '%s'",
            event.c_str()
        );
    }
}

PyObject* python_t::manifest(PyObject * self,
                             PyObject * args)
{
    PyObject * builtins = PyEval_GetBuiltins();
    PyObject * sandbox = PyDict_GetItemString(builtins, "__sandbox__");

    if(!sandbox) {
        PyErr_SetString(
            PyExc_RuntimeError,
            "Corrupted context"
        );

        return NULL;
    }

    return PyDictProxy_New(
        static_cast<python_t*>(PyCObject_AsVoidPtr(sandbox))->m_python_manifest
    );
}

// XXX: Check reference counting.
PyObject* python_t::wrap(const Json::Value& value) {
    PyObject * object = NULL;

    switch(value.type()) {
        case Json::booleanValue:
            return PyBool_FromLong(value.asBool());
        case Json::intValue:
        case Json::uintValue:
            return PyLong_FromLong(value.asInt());
        case Json::realValue:
            return PyFloat_FromDouble(value.asDouble());
        case Json::stringValue:
            return PyString_FromString(value.asCString());
        case Json::objectValue: {
            object = PyDict_New();
            Json::Value::Members names(value.getMemberNames());

            for(Json::Value::Members::iterator it = names.begin();
                it != names.end();
                ++it) 
            {
                PyDict_SetItemString(object, it->c_str(), wrap(value[*it]));
            }

            break;
        } case Json::arrayValue: {
            object = PyTuple_New(value.size());
            Py_ssize_t position = 0;

            for(Json::Value::const_iterator it = value.begin(); 
                it != value.end();
                ++it) 
            {
                PyTuple_SetItem(object, position++, wrap(*it));
            }

            break;
        } case Json::nullValue:
            Py_RETURN_NONE;
    }

    return object;
}

std::string python_t::exception() {
    tracked_object_t type(NULL), value(NULL), traceback(NULL);
    
    PyErr_Fetch(&type, &value, &traceback);

    tracked_object_t name(PyObject_Str(type));
    tracked_object_t message(PyObject_Str(value));
    
    boost::format formatter("uncaught exception %s: %s");
    
    std::string result(
        (formatter
            % PyString_AsString(name)
            % PyString_AsString(message)
        ).str()
    );

    return result;
}

extern "C" {
    void initialize(repository_t& repository) {
        repository.insert<python_t>("python");
    }
}
