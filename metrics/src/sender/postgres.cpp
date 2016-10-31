#include "cocaine/sender/postgres.hpp"

#include "cocaine/api/postgres/pool.hpp"
#include "cocaine/postgres/transaction.hpp"

#include <cocaine/context.hpp>
#include <cocaine/context/config.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/format.hpp>
#include <cocaine/logging.hpp>

#include <blackhole/logger.hpp>

#include <pqxx/transaction>

namespace cocaine {
namespace sender {

pg_sender_t::pg_sender_t(context_t& context,
                         asio::io_service& io_loop,
                         const std::string& name,
                         data_provider_ptr _data_provier,
                         const dynamic_t& args
) :
    sender_t(context, io_loop, name, nullptr, args),
    policy(policy_t::continous),
    data_provider(std::move(_data_provier)),
    pool(api::postgres::pool(context, args.as_object().at("pg_backend", "core").as_string())),
    hostname(context.config().network().hostname()),
    table_name(args.as_object().at("pg_table_name", "cocaine_metrics").as_string()),
    logger(context.log(format("pg_sender/{}", name))),
    send_period(args.as_object().at("pg_send_period_s", 1u).as_uint()),
    send_timer(io_loop),
    gc_timer(io_loop),
    gc_period(args.as_object().at("pg_gc_period_s", 3600u).as_uint()),
    gc_ttl(args.as_object().at("pg_gc_ttl_s", 3600u).as_uint())
{
    auto _policy = args.as_object().at("policy", "").as_string();
    if(!_policy.empty()) {
        if(_policy == "continous") {
            policy = policy_t::continous;
        } else if(_policy == "gc") {
            policy = policy_t::gc;
        } else if(_policy == "update") {
            policy = policy_t::update;
        } else {
            throw error_t("invalid postgress sender({}) policy specification - {}", name, _policy);
        }
    }
    if(send_period.ticks() == 0) {
        throw error_t("pg_send_period can not be zero");
    }
    send_timer.expires_from_now(send_period);
    send_timer.async_wait(std::bind(&pg_sender_t::on_send_timer, this, std::placeholders::_1));

    if(policy == policy_t::gc) {
        gc_timer.expires_from_now(gc_period);
        send_timer.async_wait(std::bind(&pg_sender_t::on_gc_timer, this, std::placeholders::_1));
    }
}

auto pg_sender_t::on_send_timer(const std::error_code& ec) -> void {
    if(!ec) {
        send(data_provider->fetch());
        send_timer.expires_from_now(send_period);
        send_timer.async_wait(std::bind(&pg_sender_t::on_send_timer, this, std::placeholders::_1));
    } else {
        COCAINE_LOG_WARNING(logger, "sender timer was cancelled");
    }
}

auto pg_sender_t::on_gc_timer(const std::error_code& ec) -> void {
    if(!ec) {
        pool->execute([=](pqxx::connection_base& connection) {
            try {
                pqxx::work transaction(connection);
                auto query = cocaine::format("DELETE FROM {} WHERE ts < now()-interval '{} seconds';",
                                             transaction.esc(table_name), gc_ttl.total_seconds());
                COCAINE_LOG_DEBUG(logger, "executing {}", query);
                auto sql_result = transaction.exec(query);
                transaction.commit();
            } catch (const std::exception& e) {
                COCAINE_LOG_ERROR(logger, "GC query failed - {}", e.what());
            }
            gc_timer.expires_from_now(gc_period);
            gc_timer.async_wait(std::bind(&pg_sender_t::on_gc_timer, this, std::placeholders::_1));
        });
    } else {
        COCAINE_LOG_WARNING(logger, "sender timer was cancelled");
    }
}


auto pg_sender_t::send(dynamic_t data) -> void {
    pool->execute([=](pqxx::connection_base& connection){
        try {
            auto data_string = boost::lexical_cast<std::string>(data);
            auto transaction = postgres::start_transaction(connection);
            switch (policy) {
                case policy_t::gc:
                case policy_t::continous: {
                    auto query = cocaine::format("INSERT INTO {} (ts, host, data) VALUES(now(), {}, {});",
                                                 transaction->esc(table_name),
                                                 transaction->quote(hostname),
                                                 transaction->quote(data_string));
                    COCAINE_LOG_DEBUG(logger, "executing {}", query);
                    auto sql_result = transaction->exec(query);
                    transaction->commit();
                    break;
                }
                case policy_t::update: {
                    auto query = cocaine::format("SELECT count(*) FROM {} WHERE host = {};",
                                                 transaction->esc(table_name),
                                                 transaction->quote(hostname));
                    COCAINE_LOG_DEBUG(logger, "executing {}", query);
                    auto sql_result = transaction->exec(query);
                    if (sql_result[0][0].as<size_t>() == 0) {
                        query = cocaine::format("INSERT INTO {} (ts, host, data) VALUES(now(), {}, {});",
                                                transaction->esc(table_name),
                                                transaction->quote(hostname),
                                                transaction->quote(data_string));
                        COCAINE_LOG_DEBUG(logger, "executing {}", query);
                        transaction->exec(query);
                    } else {
                        query = cocaine::format("UPDATE {}  set ts = now(), data = {} WHERE host = {};",
                                                transaction->esc(table_name),
                                                transaction->quote(data_string),
                                                transaction->quote(hostname));
                        COCAINE_LOG_DEBUG(logger, "executing {}", query);
                        transaction->exec(query);
                    }
                    transaction->commit();
                    break;
                }
                default:
                    throw error_t("invalid policy - {}", static_cast<int>(policy));
            }
        } catch (const std::exception& e) {
            COCAINE_LOG_ERROR(logger, "metric sending failed - {}", e.what());
        }
    });
}

} // namespace sender
} // namespace cocaine
