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

#include "storage.hpp"

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

#include <mongo/client/connpool.h>
#include <mongo/client/gridfs.h>

using namespace cocaine;
using namespace cocaine::storage;
using namespace cocaine::logging;
using namespace mongo;

typedef std::unique_ptr<ScopedDbConnection> connection_ptr_t;

mongo_storage_t::mongo_storage_t(context_t& context,
                                 const std::string& name,
                                 const Json::Value& args)
try:
    category_type(context, name, args),
    m_log(new log_t(context, name)),
    m_uri(args["uri"].asString(), ConnectionString::SET)
{
    if(!m_uri.isValid()) {
        throw storage_error_t("invalid mongodb uri");
    }
} catch(const DBException& e) {
    throw storage_error_t(e.what());
}

std::string
mongo_storage_t::read(const std::string& ns,
                      const std::string& key)
{
    std::string result;

    try {
        connection_ptr_t connection(ScopedDbConnection::getScopedDbConnection(m_uri.toString()));
        GridFS gridfs(connection->conn(), "cocaine", ns);
        GridFile file(gridfs.findFile(key));

        // Fetch the blob.
        if(!file.exists()) {
            throw storage_error_t("the specified object has not been found");
        }

        connection->done();

        std::stringstream buffer;

        file.write(buffer);
        result = buffer.str();
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }

    return result;
}

void
mongo_storage_t::write(const std::string& ns,
                       const std::string& key,
                       const std::string& blob)
{
    try {
        connection_ptr_t connection(ScopedDbConnection::getScopedDbConnection(m_uri.toString()));
        GridFS gridfs(connection->conn(), "cocaine", ns);

        // Store the blob.
        BSONObj result = gridfs.storeFile(
            static_cast<const char*>(blob.data()),
            blob.size(),
            key
        );

        connection->done();
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }
}

std::vector<std::string>
mongo_storage_t::list(const std::string& ns) {
    std::vector<std::string> result;

    try {
        connection_ptr_t connection(ScopedDbConnection::getScopedDbConnection(m_uri.toString()));
        GridFS gridfs(connection->conn(), "cocaine", ns);
        std::shared_ptr<DBClientCursor> cursor(gridfs.list());
        BSONObj object;

        while(cursor->more()) {
            object = cursor->nextSafe();
            result.push_back(object["filename"].String()); // WARNING: is key always a string?
        }

        connection->done();
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }

    return result;
}

void
mongo_storage_t::remove(const std::string& ns,
                        const std::string& key)
{
    try {
        connection_ptr_t connection(ScopedDbConnection::getScopedDbConnection(m_uri.toString()));

        GridFS gridfs(connection->conn(), "cocaine", ns);

        // Remove the blob.
        gridfs.removeFile(key);

        connection->done();
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }
}

