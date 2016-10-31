#pragma once

#include "cocaine/api/postgres/pool.hpp"

#include <cocaine/forwards.hpp>
#include <cocaine/locked_ptr.hpp>

#include <asio/io_service.hpp>

#include <boost/optional/optional.hpp>

#include <pqxx/connection>

#include <thread>

namespace cocaine {
namespace postgres {

class pool_t : public api::postgres::pool_t{
public:
    pool_t(context_t& context, const std::string& name, const dynamic_t& args);

    ~pool_t();

    void
    execute(std::function<void(pqxx::connection_base&)> function);

private:
    class slot_t
    {
    public:
        typedef std::unique_ptr<pqxx::asyncconnection> connection_ptr;

        slot_t(asio::io_service& _io_loop, const std::string& connection_string);
        ~slot_t();

        std::thread::id
        id() const;

        void
        execute(std::function<void(pqxx::connection_base&)> function);

    private:
        connection_ptr
        make_connection(const std::string& connection_string);

        connection_ptr connection;
        std::thread thread;
    };

    // shared_ptr is here because map can not handle move-only objects properly
    typedef std::map<std::thread::id, std::shared_ptr<slot_t>> slots_t;

    asio::io_service io_loop;
    boost::optional<asio::io_service::work> io_work;
    synchronized<slots_t> slots;
};

} // namespace postgres
} // namespace cocaine
