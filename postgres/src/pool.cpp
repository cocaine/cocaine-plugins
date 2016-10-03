#include "cocaine/postgres/pool.hpp"

#include <thread>

namespace cocaine {
namespace postgres {

pool_t::pool_t(size_t pool_size, const std::string& connection_string) :
    io_loop(),
    io_work(asio::io_service::work(io_loop))
{
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
