#include "cocaine/postgres/pool.hpp"

#include <thread>

namespace cocaine {
namespace postgres {

pool_t::pool_t(context_t& context, const std::string& name, const dynamic_t& args) :
    api::postgres::pool_t(context, name, args),
    io_loop(),
    io_work(asio::io_service::work(io_loop))
{
    size_t pool_size = args.as_object().at("pool_size", 1u).as_uint();
    std::string connection_string = args.as_object().at("connection_string", "").as_string();
    slots.apply([&](slots_t& _slots){
        for(size_t i = 0; i < pool_size; i++) {
            auto slot = std::make_shared<slot_t>(io_loop, connection_string);
            auto id = slot->id();
            _slots.emplace(id, std::move(slot));
        }
    });
}

pool_t::~pool_t() {
    io_work.reset();
    slots->clear();
}

void
pool_t::execute(std::function<void(pqxx::connection_base&)> function) {
    io_loop.post([=](){
        auto& slot = slots->at(std::this_thread::get_id());
        slot->execute(std::move(function));
    });
}

pool_t::slot_t::slot_t(asio::io_service& _io_loop, const std::string& connection_string) :
    connection(make_connection(connection_string)),
    thread([&](){
        _io_loop.run();
    })
{}

pool_t::slot_t::~slot_t() {
    thread.join();
}

std::thread::id
pool_t::slot_t::id() const {
    return thread.get_id();
}

void
pool_t::slot_t::execute(std::function<void(pqxx::connection_base&)> function) {
    function(*connection);
}

pool_t::slot_t::connection_ptr
pool_t::slot_t::make_connection(const std::string& connection_string) {
    return connection_ptr(new pqxx::asyncconnection(connection_string));
}

} // namespace postgres
} // namespace cocaine
