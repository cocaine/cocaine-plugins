#pragma once

#include <pqxx/transaction>

namespace cocaine {
namespace postgres {

inline std::unique_ptr<pqxx::work> start_transaction(pqxx::connection_base& c, size_t attempt_count = 3) {
    for(size_t i = 0; i < attempt_count; i++) {
        try {
            return std::unique_ptr<pqxx::work>(new pqxx::work(c));
        } catch (const pqxx::broken_connection& e) {
            if(i == attempt_count - 1) {
                throw;
            }
        }
    }
    //Unreachable, but compiler still complain without assert
    assert(false);
}

}
}
