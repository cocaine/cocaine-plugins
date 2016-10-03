/*
* 2016+ Copyright (c) Anton Matveenko <antmat@yandex-team.ru>
* All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*/

#pragma once

#include "cocaine/postgres/pool.hpp"

#include <cocaine/api/storage.hpp>

#include <pqxx/pqxx>

namespace cocaine {
namespace storage {

/**
 * This class provide a wrapper over storage to store index data.
 * Real data is stored in the underlying storage,
 * read requests are proxied to wrapped storage
 * write requests are proxied to wrapped storage with empty taglist and tag data is stored in PG
 **/
class postgres_t : public api::storage_t {
public:
    template<class T>
    using callback = std::function<void(std::future<T>)>;

    /**
     * construct a Postgres index wrapper. Param in args
     * pg_table_name - table name to store indexes, default "cocaine_index"
     * pg_underlying_storage - name of wrapped storage fetched from context, default "core"
     * pg_pool_size - pg connection pool size, default 1
     * pg_connection_string - connection param for pg client
     * pg_key_column - column name for storing key, default 'key'
     * pg_collection_column - column name for storing collection, default 'collection'
     * pg_tags_column - column name for storing tags in json, default 'tags'
     */
    postgres_t(context_t& context, const std::string& name, const dynamic_t& args);

    using api::storage_t::read;

    virtual void
    read(const std::string& collection, const std::string& key, callback<std::string> cb);

    using api::storage_t::write;

    virtual void
    write(const std::string& collection,
          const std::string& key,
          const std::string& blob,
          const std::vector<std::string>& tags,
          callback<void> cb);

    using api::storage_t::remove;

    virtual void
    remove(const std::string& collection, const std::string& key, callback<void> cb);

    using api::storage_t::find;

    virtual void
    find(const std::string& collection, const std::vector<std::string>& tags, callback<std::vector<std::string>> cb);

private:
    std::shared_ptr<logging::logger_t> log;

    std::string collection_column_name;
    std::string key_column_name;
    std::string tags_column_name;

    std::string table_name;
    api::storage_ptr wrapped;
    postgres::pool_t pg_pool;
};
} // namespace storage
} // namespace cocaine
