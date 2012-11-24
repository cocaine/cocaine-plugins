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

#include "storage.hpp"

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

using namespace cocaine;
using namespace cocaine::storage;

log_adapter_t::log_adapter_t(const boost::shared_ptr<logging::logger_t>& log,
                             const int level):
    ioremap::elliptics::logger(level),
    m_log(log),
    m_level(level)
{ }

void
log_adapter_t::log(const int level,
                   const char * message)
{
    switch(level) {
        case DNET_LOG_DEBUG:
            COCAINE_LOG_DEBUG(m_log, "%s", message);
            break;

        case DNET_LOG_NOTICE:
            COCAINE_LOG_INFO(m_log, "%s", message);
            break;

        case DNET_LOG_INFO:
            COCAINE_LOG_INFO(m_log, "%s", message);
            break;

        case DNET_LOG_ERROR:
            COCAINE_LOG_ERROR(m_log, "%s", message);
            break;

        default:
            break;
    };
}

unsigned long
log_adapter_t::clone() {
    return reinterpret_cast<unsigned long>(
        new log_adapter_t(m_log, m_level)
    );
}

namespace {
    struct digitizer {
        template<class T>
        int
        operator()(const T& value) {
            return value.asInt();
        }
    };
}

elliptics_storage_t::elliptics_storage_t(context_t& context,
                                         const std::string& name,
                                         const Json::Value& args):
    category_type(context, name, args),
    m_context(context),
    m_log(context.log(name)),
    m_log_adapter(m_log, args.get("verbosity", DNET_LOG_ERROR).asUInt()),
    m_node(m_log_adapter),
    m_session(m_node)
{
    Json::Value nodes(args["nodes"]);

    if(nodes.empty() || !nodes.isObject()) {
        throw configuration_error_t("no nodes has been specified");
    }

    Json::Value::Members node_names(nodes.getMemberNames());

    for(Json::Value::Members::const_iterator it = node_names.begin();
        it != node_names.end();
        ++it)
    {
        try {
            m_node.add_remote(
                it->c_str(),
                nodes[*it].asInt()
            );
        } catch(const std::runtime_error& e) {
            // Do nothing. Yes. Really. We only care if no remote nodes were added at all.
        }
    }

    Json::Value groups(args["groups"]);

    if(groups.empty() || !groups.isArray()) {
        throw configuration_error_t("no groups has been specified");
    }

    std::transform(
        groups.begin(),
        groups.end(),
        std::back_inserter(m_groups),
        digitizer()
    );

    m_session.add_groups(m_groups);
}

std::string
elliptics_storage_t::read(const std::string& collection,
                          const std::string& key)
{
    std::string blob;

    COCAINE_LOG_DEBUG(
        m_log,
        "reading the '%s' object, collection: '%s'",
        key,
        collection
    );

    try {
        blob = m_session.read_data_wait(id(collection, key), 0, 0, 0, 0, 0);
    } catch(const std::runtime_error& e) {
        throw storage_error_t(e.what());
    }

    return blob;
}

void
elliptics_storage_t::write(const std::string& collection,
                           const std::string& key,
                           const std::string& blob)
{
    struct dnet_id dnet_id;
    struct timespec ts = { 0, 0 };

    // NOTE: Elliptcs does not initialize the contents of the keys. 
    memset(&dnet_id, 0, sizeof(struct dnet_id));

    COCAINE_LOG_DEBUG(
        m_log,
        "writing the '%s' object, collection: '%s'",
        key,
        collection
    );
    
    try {
        // Generate the key.
        m_session.transform(
            id(collection, key),
            dnet_id
        );

        // Write the blob.
        m_session.write_data_wait(dnet_id, blob, 0, 0, 0);

        // Write the blob metadata.
        m_session.write_metadata(
            dnet_id,
            id(collection, key),
            m_groups,
            ts,
            0
        );

        // Check if the key already exists in the collection.
        std::vector<std::string> keylist(
            list(collection)
        );
        
        if(std::find(keylist.begin(), keylist.end(), key) == keylist.end()) {
            msgpack::sbuffer buffer;
            std::string object;
            
            keylist.push_back(key);
            msgpack::pack(&buffer, keylist);
            
            object.assign(
                buffer.data(),
                buffer.size()
            );

            // Generate the collection object key.
            m_session.transform(
                id("system", "list:" + collection),
                dnet_id
            );

            // Update the collection object.
            m_session.write_data_wait(dnet_id, object, 0, 0, 0);

            // Update the collection object metadata.
            m_session.write_metadata(
                dnet_id,
                id("system", "list:" + collection),
                m_groups,
                ts,
                0
            );
        }
    } catch(const std::runtime_error& e) {
        throw storage_error_t(e.what());
    }
}

std::vector<std::string>
elliptics_storage_t::list(const std::string& collection) {
    std::vector<std::string> result;
    std::string blob;
    
    try {
        blob = m_session.read_data_wait(id("system", "list:" + collection), 0, 0, 0, 0, 0);
    } catch(const std::runtime_error& e) {
        return result;
    }

    msgpack::unpacked unpacked;

    try {
        msgpack::unpack(&unpacked, blob.data(), blob.size());
        unpacked.get().convert(&result);
    } catch(const msgpack::unpack_error& e) {
        throw storage_error_t("the collection metadata is corrupted");
    } catch(const msgpack::type_error& e) {
        throw storage_error_t("the collection metadata is corrupted");
    }

    return result;
}

void
elliptics_storage_t::remove(const std::string& collection,
                            const std::string& key)
{
    struct dnet_id dnet_id;
    struct timespec ts = { 0, 0 };
    
    // NOTE: Elliptcs does not initialize the contents of the keys. 
    memset(&dnet_id, 0, sizeof(struct dnet_id));
    
    COCAINE_LOG_DEBUG(
        m_log,
        "removing the '%s' object, collection: '%s'",
        key,
        collection
    );
    
    try {
        std::vector<std::string> keylist(list(collection)),
                                 updated;

        std::remove_copy(
            keylist.begin(),
            keylist.end(),
            std::back_inserter(updated),
            key
        );

        msgpack::sbuffer buffer;
        std::string object;

        msgpack::pack(&buffer, updated);
        object.assign(buffer.data(), buffer.size());

        // Generate the collection object key.
        m_session.transform(
            id("system", "list:" + collection),
            dnet_id
        );

        // Update the collection object.
        m_session.write_data_wait(dnet_id, object, 0, 0, 0);

        // Update the collection object metadata.
        m_session.write_metadata(
            dnet_id,
            id("system", "list:" + collection),
            m_groups,
            ts,
            0
        );

        // Remove the actual key.
        m_session.remove(id(collection, key));
    } catch(const std::runtime_error& e) {
        throw storage_error_t(e.what());
    }
}

