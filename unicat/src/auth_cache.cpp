#include "auth_cache.hpp"

namespace cocaine { namespace unicat { namespace authorization {

namespace {

template<typename Access>
auto make_by_type(context_t& context, const std::string& name) -> std::shared_ptr<Access>;

template<>
auto make_by_type<api::authorization::unicorn_t>(context_t& context, const std::string& name)
    -> std::shared_ptr<api::authorization::unicorn_t>
{
    return api::authorization::unicorn(context, name);
}

template<>
auto make_by_type<api::authorization::storage_t>(context_t& context, const std::string& name)
    -> std::shared_ptr<api::authorization::storage_t>
{
    return api::authorization::storage(context, name);
}

template<typename Access, typename Map>
auto
get_or_insert(Map& auth_cache, context_t& context, const std::string& name) -> std::shared_ptr<Access>
{
    return auth_cache.apply([&] (std::map<std::string, std::shared_ptr<Access>>& cache) -> std::shared_ptr<Access> {
        auto result = cache.find(name);
        if (result == std::end(cache)) {
            auto access = make_by_type<Access>(context, name);
            cache.emplace(name, access);
            return access;
        }

        return result->second;
    });
}

} // anon namespace

handlers_cache_t::handlers_cache_t(context_t& context):
    context(context)
{}


template<>
auto
handlers_cache_t::get_cache_ref()
    -> synchronized<std::map<std::string, std::shared_ptr<api::authorization::storage_t>>>&
{
    return storages;
}

template<>
auto
handlers_cache_t::get_cache_ref()
    -> synchronized<std::map<std::string, std::shared_ptr<api::authorization::unicorn_t>>>&
{
    return unicorns;
}

template<typename Access>
auto
handlers_cache_t::make_handler(const std::string& name) -> std::shared_ptr<Access> {
    return get_or_insert<Access>( get_cache_ref<Access>(), context, name);
}

template
auto handlers_cache_t::make_handler(const std::string& name)
    -> std::shared_ptr<api::authorization::storage_t>;

template
auto handlers_cache_t::make_handler(const std::string& name)
    -> std::shared_ptr<api::authorization::unicorn_t>;
}



}
}
