/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <future>

#include <cocaine/api/service.hpp>
#include <cocaine/idl/context.hpp>
#include <cocaine/locked_ptr.hpp>
#include <cocaine/rpc/dispatch.hpp>

#include "cocaine/idl/node.hpp"

#include "node/forwards.hpp"

namespace cocaine {
namespace service {

/// Node Service is responsible for managing applications.
class node_t :
    public api::service_t,
    public dispatch<io::node_tag>
{
public:
    using callback_type = std::function<void(std::future<void> future)>;

private:
    const std::shared_ptr<logging::logger_t> log;

    context_t& context;

    // Started applications.
    synchronized<std::map<std::string, std::shared_ptr<node::app_t>>> apps;

    // Slot for context signals.
    std::shared_ptr<dispatch<io::context_tag>> signal;

public:
    node_t(context_t& context, asio::io_service& loop, const std::string& name, const dynamic_t& args);
    ~node_t();

    auto prototype() const -> const io::basic_dispatch_t& override;

    auto list() const -> dynamic_t;

    auto
    start_app(const std::string& name, const std::string& profile, callback_type callback) -> void;

    auto pause_app(const std::string& name) -> void;

    // TODO: Flags are bad!
    auto info(const std::string& name, io::node::info::flags_t flags) const -> dynamic_t;

    auto overseer(const std::string& name) const -> std::shared_ptr<node::overseer_t>;

private:
    auto
    start_app(const std::string& name, const std::string& profile) -> deferred<void>;

    auto on_context_shutdown() -> void;
};

}  // namespace service
}  // namespace cocaine

namespace cocaine {
namespace error {

auto node_category() -> const std::error_category&;

}  // namespace error
}  // namespace cocaine
