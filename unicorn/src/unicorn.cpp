/*
* 2015+ Copyright (c) Anton Matveenko <antmat@yandex-team.ru>
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

#include "cocaine/unicorn.hpp"
#include "cocaine/unicorn/api/zookeeper.hpp"

#include <cocaine/context.hpp>

using namespace cocaine::unicorn;

namespace cocaine {
namespace service {

const io::basic_dispatch_t&
unicorn_service_t::prototype() const {
    return *this;
}

unicorn_service_t::unicorn_service_t(context_t& context, asio::io_service& _asio, const std::string& _name, const dynamic_t& args) :
    service_t(context, _asio, _name, args),
    dispatch<io::unicorn_tag>(_name),
    name(_name),
    zk_session(),
    zk(make_zk_config(args), zk_session),
    log(context.log("unicorn")),
    api(*log, zk)
{
    using namespace std::placeholders;

    on<io::unicorn::subscribe>         (std::make_shared<subscribe_slot_t>         (this, &unicorn::api_t::subscribe));
    on<io::unicorn::children_subscribe>(std::make_shared<children_subscribe_slot_t>(this, &unicorn::api_t::children_subscribe));
    on<io::unicorn::put>               (std::make_shared<put_slot_t>               (this, &unicorn::api_t::put));
    on<io::unicorn::create>            (std::make_shared<create_slot_t>            (this, &unicorn::api_t::create_default));
    on<io::unicorn::del>               (std::make_shared<del_slot_t>               (this, &unicorn::api_t::del));
    on<io::unicorn::increment>         (std::make_shared<increment_slot_t>         (this, &unicorn::api_t::increment));
    on<io::unicorn::lock>              (std::make_shared<lock_slot_t>              (this, &unicorn::api_t::lock));

}

unicorn_dispatch_t::unicorn_dispatch_t(const std::string& name, unicorn_service_t* service) :
    dispatch<io::unicorn_final_tag>(name),
    api(new zookeeper_api_t(*service->log, service->zk))
{
}

}}
