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

#ifndef COCAINE_PYTHON_SANDBOX_EVENT_SOURCE_HPP
#define COCAINE_PYTHON_SANDBOX_EVENT_SOURCE_HPP

// NOTE: These are being redefined in Python.h
#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE

#include "Python.h"

#include <cocaine/common.hpp>

namespace cocaine { namespace sandbox {

struct event_source_t {
    virtual
    ~event_source_t();

    void
    on(const std::string& event,
       PyObject * callback);

    bool
    invoke(const std::string& event,
           PyObject * args,
           PyObject * kwargs);

private:
    typedef std::vector<
        PyObject*
    > slot_t;

#if BOOST_VERSION >= 103600
    typedef boost::unordered_map<
#else
    typedef std::map<
#endif
        std::string,
        slot_t
    > slot_map_t;

    slot_map_t m_slots;
};

}}


#endif
