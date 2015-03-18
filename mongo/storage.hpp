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

#ifndef COCAINE_MONGO_STORAGE_HPP
#define COCAINE_MONGO_STORAGE_HPP

#include <cocaine/api/storage.hpp>
#include <mongo/client/dbclient.h>

namespace cocaine { namespace storage {

class mongo_storage_t:
    public api::storage_t
{
    const std::unique_ptr<logging::log_t> m_log;

    // Should be const, but is not constructible in initialization list due to stupid design.
    std::unique_ptr<mongo::DBClientBase> m_client;

public:
    mongo_storage_t(context_t& context, const std::string& name, const dynamic_t& args);

    virtual
    std::string
    read(const std::string& collection, const std::string& key);

    virtual
    void
    write(const std::string& collection, const std::string& key, const std::string& blob,
          const std::vector<std::string>& tags);

    virtual
    std::vector<std::string>
    find(const std::string& collection, const std::vector<std::string>& tags);

    virtual
    void
    remove(const std::string& collection, const std::string& key);
};

}}

#endif
