#pragma once

#include <map>
#include <memory>
#include <string>

#include <cocaine/context.hpp>
#include <cocaine/locked_ptr.hpp>

#include <cocaine/api/authorization/storage.hpp>
#include <cocaine/api/authorization/unicorn.hpp>

namespace cocaine { namespace unicat { namespace authorization {

//
// TODO: seems overdesigned and redundant, should be simplified, refactor out!
//
// Store auth backends in cache for service wide usage, it helps to avoid
// authorization structure (within backend) to be deleted inside self
// executer thread due race conditions on async function exception: smart
// pointer to backend is held by async callback and on exception it is last
// owner of pointer, so backend was deleted after exit from callback and within
// callback (executor) thread, but callback thread itself was owned by
// authorization, so it lead to 'self join exception' on thread deletion.
class handlers_cache_t {
    using storages_mapping_type = std::map<std::string, std::shared_ptr<api::authorization::storage_t>>;
    using unicorns_mapping_type = std::map<std::string, std::shared_ptr<api::authorization::unicorn_t>>;

    synchronized<storages_mapping_type> storages;
    synchronized<unicorns_mapping_type> unicorns;

    context_t& context;
public:
    explicit handlers_cache_t(context_t& context);

    template<typename Access>
    auto make_handler(const std::string& name) -> std::shared_ptr<Access>;
private:
    template<typename Access>
    auto get_cache_ref() -> synchronized<std::map<std::string, std::shared_ptr<Access>>>&;
};

}
}
}
