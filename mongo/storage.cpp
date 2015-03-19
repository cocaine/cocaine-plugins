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

#include <mongo/client/gridfs.h>

using namespace cocaine;
using namespace cocaine::storage;
using namespace cocaine::logging;
using namespace mongo;

namespace {

std::unique_ptr<DBClientBase>
connect(const std::string& uri) {
    std::string error;

    mongo::ConnectionString connection_string(uri, ConnectionString::SET);
    mongo::DBClientBase* ptr = nullptr;

    try {
        // It will automatically determine the MongoDB cluster setup.
        ptr = connection_string.connect(error);

        if(ptr == nullptr) {
            throw storage_error_t(error);
        }
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }

    return std::unique_ptr<DBClientBase>(ptr);
}

} // namespace

mongo_storage_t::mongo_storage_t(context_t& context, const std::string& name, const dynamic_t& args):
    category_type(context, name, args),
    m_log(context.log(name)),
    m_client(connect(args.as_object().at("uri", "").to<std::string>()))
{ }

std::string
mongo_storage_t::read(const std::string& collection, const std::string& key) {
    std::stringstream buffer;

    try {
        GridFS gridfs(*m_client, "cocaine", "fs." + collection);
        GridFile file(gridfs.findFile(key));

        if(!file.exists()) {
            throw storage_error_t("the specified object has not been found");
        }

        file.write(buffer);
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }

    return buffer.str();
}

void
mongo_storage_t::write(const std::string& collection, const std::string& key, const std::string& blob,
                       const std::vector<std::string>& tags)
{
    try {
        BSONObjBuilder meta;
        GridFS gridfs(*m_client, "cocaine", "fs." + collection);

        meta.append("key", key);
        meta.append("tags", tags);

        // Store tags.
        // Add object {"key": key, "tags": tags} to collection 'meta.collection' in 'cocaine' database.
        m_client->insert("cocaine.meta." + collection, meta.done());

        // Store the blob.
        gridfs.storeFile(
            static_cast<const char*>(blob.data()),
            blob.size(),
            key
        );
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }
}

std::vector<std::string>
mongo_storage_t::find(const std::string& collection, const std::vector<std::string>& tags) {
    std::vector<std::string> result;

    try {
        // Build a query to mongodb: {"$and": [{"tags": "tag1"}, {"tags": "tag2"}, {"tags": "tag3"}]}.
        std::vector<BSONObj> tag_queries;

        for (auto tag = tags.begin(); tag != tags.end(); ++tag) {
            BSONObjBuilder tag_query;

            tag_query.append("tags", *tag);
            tag_queries.push_back(tag_query.obj());
        }

        BSONObjBuilder query;
        query.append("$and", tag_queries);

        BSONObj object;

        // Fetch keys.
        std::unique_ptr<DBClientCursor> cursor = m_client->query("cocaine.meta." + collection, query.done());

        while(cursor->more()) {
            object = cursor->nextSafe();
            result.push_back(object["key"].String());
        }
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }

    return result;
}

void
mongo_storage_t::remove(const std::string& collection, const std::string& key) {
    try {
        BSONObjBuilder meta;
        GridFS gridfs(*m_client, "cocaine", "fs." + collection);

        meta.append("key", key);

        // Remove the information about tags.
        m_client->remove("cocaine.meta." + collection, Query(meta.done()));

        // Remove the blob.
        gridfs.removeFile(key);
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }
}

