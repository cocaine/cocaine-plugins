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

#include <cerrno>

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

#include <mongo/client/gridfs.h>

using namespace cocaine;
using namespace cocaine::storage;
using namespace cocaine::logging;
using namespace mongo;

namespace {

std::unique_ptr<DBClientBase> connect(const std::string& uri) {
    std::string error;

    mongo::ConnectionString connection_string(uri, ConnectionString::SET);
    mongo::DBClientBase* ptr = nullptr;

    try {
        // It will automatically determine the MongoDB cluster setup.
        ptr = connection_string.connect(error);

        if(ptr == nullptr) {
            throw std::system_error(std::error_code(-100, std::generic_category()), error);
        }
    } catch(const DBException& e) {
        throw std::system_error(std::error_code(-100, std::generic_category()), e.what());
    }

    return std::unique_ptr<DBClientBase>(ptr);
}

} // namespace

mongo_storage_t::mongo_storage_t(context_t& context, const std::string& name, const dynamic_t& args):
    category_type(context, name, args),
    m_log(context.log(name)),
    m_client(connect(args.as_object().at("uri", "<unspecified>").to<std::string>()))
{ }

std::string
mongo_storage_t::read(const std::string& collection, const std::string& key) {
    std::stringstream buffer;

    try {
        GridFS gridfs(*m_client, "cocaine", "fs." + collection);

        // A better way to do this is to use the std::string overload for GridFS::findFile(), but as
        // of legacy client version 1.0.0, this part of client API is broken.
        GridFile file(gridfs.findFile(BSONObjBuilder()
            .append("filename", key)
            .obj()
        ));

        if(!file.exists()) {
            throw std::system_error(std::error_code(-ENOENT, std::system_category()), "the specified object has not been found");
        }

        file.write(buffer);
    } catch(const DBException& e) {
        throw std::system_error(std::error_code(-100, std::generic_category()), e.what());
    }

    return buffer.str();
}

void
mongo_storage_t::write(const std::string& collection, const std::string& key, const std::string& blob,
                       const std::vector<std::string>& tags)
{
    try {
        GridFS gridfs(*m_client, "cocaine", "fs." + collection);

        // Store blob tags.
        m_client->insert("cocaine.meta." + collection, BSONObjBuilder()
            .append("key" , key )
            .append("tags", tags)
            .obj()
        );

        // Store the blob itself.
        gridfs.storeFile(static_cast<const char*>(blob.data()), blob.size(), key);
    } catch(const DBException& e) {
        throw std::system_error(std::error_code(-100, std::generic_category()), e.what());
    }
}

std::vector<std::string>
mongo_storage_t::find(const std::string& collection, const std::vector<std::string>& tags) {
    std::vector<std::string> result;
    std::vector<BSONObj> tag_queries;

    for(auto tag = tags.begin(); tag != tags.end(); ++tag) {
        tag_queries.push_back(BSONObjBuilder().append("tags", *tag).obj());
    }

    BSONObj object;

    try {
        // Fetch all the keys which have all of the specified tags, i.e. the resulting query will be
        // something like this: {"$and": [{"tags": "tag_1"}, {"tags": "tag_2"}, {"tags": "tag_3"}]}.
        auto cursor = m_client->query("cocaine.meta." + collection, BSONObjBuilder()
            .append("$and", tag_queries)
            .obj()
        );

        while(cursor->more()) {
            object = cursor->nextSafe();
            result.push_back(object["key"].String());
        }
    } catch(const DBException& e) {
        throw std::system_error(std::error_code(-100, std::generic_category()), e.what());
    }

    return result;
}

void
mongo_storage_t::remove(const std::string& collection, const std::string& key) {
    try {
        GridFS gridfs(*m_client, "cocaine", "fs." + collection);

        // Remove the information about tags.
        m_client->remove("cocaine.meta." + collection, BSONObjBuilder()
            .append("key", key)
            .obj()
        );

        // Remove the blob.
        gridfs.removeFile(key);
    } catch(const DBException& e) {
        throw std::system_error(std::error_code(-100, std::generic_category()), e.what());
    }
}

