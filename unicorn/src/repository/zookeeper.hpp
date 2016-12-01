#pragma once

#include <blackhole/logger.hpp>

#include <cocaine/locked_ptr.hpp>
#include <cocaine/repository.hpp>
#include <cocaine/repository/unicorn.hpp>

#include "cocaine/detail/unicorn/zookeeper.hpp"

namespace cocaine {
namespace api {

struct zookeeper_factory_t :
    public category_traits<unicorn_t>::default_factory<unicorn::zookeeper_t>
{
    typedef unicorn_ptr ptr_type;

    ptr_type
    get(context_t& context, const std::string& name, const dynamic_t& args) override;
};

template<>
struct plugin_traits<unicorn::zookeeper_t> {
    typedef zookeeper_factory_t factory_type;
};

}  // namespace api
}  // namespace cocaine
