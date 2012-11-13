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

#include "event_source.hpp"

#include "sandbox.hpp"

using namespace cocaine::sandbox;

namespace {
    struct decrement_reference_t {
        void operator()(PyObject * object) {
            Py_DECREF(object);
        }
    };

    struct dispose_t {
        template<class T>
        void
        operator()(T& slot) {
            std::for_each(slot.second.begin(), slot.second.end(), decrement_reference_t());
        }
    };
}

event_source_t::~event_source_t() {
    std::for_each(m_slots.begin(), m_slots.end(), dispose_t());
}

void
event_source_t::on(const std::string& event,
                   PyObject * callback)
{
    slot_t& slot = m_slots[event];

    if(std::find(slot.begin(), slot.end(), callback) != slot.end()) {
        throw cocaine::error_t("duplicate callback");
    }

    Py_INCREF(callback);

    slot.emplace_back(callback);
}

bool
event_source_t::invoke(const std::string& event,
                       PyObject * args,
                       PyObject * kwargs)
{
    slot_t& slot = m_slots[event];

    if(slot.empty()) {
        return false;
    }

    for(slot_t::const_iterator it = slot.begin();
        it != slot.end();
        ++it)
    {
        // NOTE: We can safely drop result here, as it's not needed.
        tracked_object_t result = PyObject_Call(*it, args, kwargs);
    
        if(PyErr_Occurred()) {
            throw cocaine::error_t(exception());
        }
    }
}

