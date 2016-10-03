#include "cocaine/storage/postgres.hpp"
#include "cocaine/error/postgres.hpp"

#include <cocaine/context.hpp>
#include <cocaine/dynamic.hpp>
#include <cocaine/logging.hpp>

#include <blackhole/logger.hpp>

#include <pqxx/except.hxx>

namespace cocaine {
namespace storage {

postgres_t::postgres_t(context_t& context, const std::string& name, const dynamic_t& args) :
    api::storage_t(context, name, args),
    log(context.log("storage/postgres/" + name)),
    collection_column_name(args.as_object().at("pg_collection_column", "collection").as_string()),
    key_column_name(args.as_object().at("pg_key_column", "key").as_string()),
    tags_column_name(args.as_object().at("pg_tags_column", "tags").as_string()),
    table_name(args.as_object().at("pg_table_name", "cocaine_index").as_string()),
    wrapped(api::storage(context, args.as_object().at("pg_underlying_storage", "core").as_string())),
    pg_pool(args.as_object().at("pg_pool_size", 1ul).as_uint(),
            args.as_object().at("pg_connection_string", "").as_string())
{}

void
postgres_t::read(const std::string& collection, const std::string& key, callback<std::string> cb) {
    COCAINE_LOG_DEBUG(log, "reading {}/{} via wrapped storage", collection, key);
    wrapped->read(collection, key, std::move(cb));
}

void
postgres_t::write(const std::string& collection,
                   const std::string& key,
                   const std::string& blob,
                   const std::vector<std::string>& tags,
                   callback<void> cb)
{
    COCAINE_LOG_DEBUG(log, "writing {}/{} via wrapped storage", collection, key);
    wrapped->write(collection, key, blob, {}, [=](std::future<void> fut){
        try {
            COCAINE_LOG_DEBUG(log, "enqueuing index setup for {}/{}", collection, key);
            // First we check if underlying write succeed
            fut.get();
            pg_pool.execute([=](pqxx::connection_base& connection){
                try {
                    // TODO: possible overhead with dynamic can be removed via usage
                    // of RapidJson or even manual formatting
                    dynamic_t tags_obj(tags);
                    auto tag_string = boost::lexical_cast<std::string>(tags_obj);
                    pqxx::work transaction(connection);
                    std::string query("INSERT INTO " + transaction.esc(table_name) + "(" +
                                          transaction.esc(collection_column_name) + ", " +
                                          transaction.esc(key_column_name) + ", " +
                                          transaction.esc(tags_column_name) + ") " +
                                      " VALUES(" +
                                          transaction.quote(collection) + ", " +
                                          transaction.quote(key) + ", " +
                                          transaction.quote(tag_string) +
                                      ") ON CONFLICT (key, collection) DO UPDATE SET " +
                                          transaction.esc(tags_column_name) + " = " + transaction.quote(tag_string) +
                                      ";");
                    COCAINE_LOG_DEBUG(log, "executing {}", query);
                    transaction.exec(query);
                    transaction.commit();
                    cb(make_ready_future());
                } catch (const std::exception& e) {
                    COCAINE_LOG_ERROR(log, "error during index update - {}", e.what());
                    cb(make_exceptional_future<void>(make_error_code(error::unknown_pg_error), e.what()));
                }
            });
        } catch(...) {
            cb(make_exceptional_future<void>());
        }
    });
}

void
postgres_t::remove(const std::string& collection, const std::string& key, callback<void> cb) {
    wrapped->remove(collection, key, [=](std::future<void> fut){
        try {
            // First we check if underlying remove succeed
            fut.get();
            pg_pool.execute([=](pqxx::connection_base& connection){
                try {
                    pqxx::work transaction(connection);
                    std::string query("DELETE FROM " + transaction.esc(table_name) + "WHERE " +
                                      transaction.esc(collection_column_name) + " = " + transaction.quote(collection) +
                                      " AND " + transaction.esc(key_column_name) + " = " + transaction.quote(key) + ";");
                    COCAINE_LOG_DEBUG(log, "executing {}", query);
                    transaction.exec(query);
                    transaction.commit();
                    cb(make_ready_future());
                } catch (const std::exception& e) {
                    COCAINE_LOG_ERROR(log, "error during index delete - {}", e.what());
                    cb(make_exceptional_future<void>(make_error_code(error::unknown_pg_error), e.what()));
                }
            });
        } catch(...) {
            cb(make_exceptional_future<void>());
        }
    });
}

void
postgres_t::find(const std::string& collection, const std::vector<std::string>& tags, callback<std::vector<std::string>> cb) {
    pg_pool.execute([=](pqxx::connection_base& connection){
        try {
            // TODO: possible overhead with dynamic can be removed via usage
            // of RapidJson or even manual formatting
            dynamic_t tags_obj(tags);
            auto tag_string = boost::lexical_cast<std::string>(tags_obj);

            pqxx::work transaction(connection);
            std::string query("SELECT " + transaction.esc(key_column_name) + " FROM " + transaction.esc(table_name) +
                              " WHERE " +
                                  transaction.esc(collection_column_name) + " = " + transaction.quote(collection) +
                                  " AND " +
                                  transaction.esc(tags_column_name) + " @> " + transaction.quote(tag_string) +
                              ";");
            COCAINE_LOG_DEBUG(log, "executing {}", query);
            auto sql_result = transaction.exec(query);

            std::vector<std::string> result;
            for(const auto& row : sql_result) {
                std::string key;
                row.at(0).to(key);
                result.push_back(std::move(key));
            }

            cb(make_ready_future(std::move(result)));
        } catch (const std::exception& e) {
            COCAINE_LOG_ERROR(log, "error during index lookup - {}", e.what());
            cb(make_exceptional_future<std::vector<std::string>>(make_error_code(error::unknown_pg_error), e.what()));
        }
    });
}

} // namespace storage
} // namespace cocaine
