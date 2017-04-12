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

#ifndef COCAINE_EXTERNAL_ISOLATE_HPP
#define COCAINE_EXTERNAL_ISOLATE_HPP

#include "cocaine/api/isolate.hpp"

#include <memory>

namespace cocaine { namespace isolate {

class external_t:
    public api::isolate_t,
    public std::enable_shared_from_this<external_t>
{
public:
    external_t(context_t& context, asio::io_service& io_context, const std::string& name, const std::string& type, const dynamic_t& args);

    virtual
    std::unique_ptr<api::cancellation_t>
    spool(std::shared_ptr<api::spool_handle_base_t> handler);

    virtual
    std::unique_ptr<api::cancellation_t>
    spawn(const std::string& path,
                const api::args_t& args,
                const api::env_t& environment,
                std::shared_ptr<api::spawn_handle_base_t>);

    virtual
    void
    metrics(const dynamic_t& query, std::shared_ptr<api::metrics_handle_base_t> handle) const override;

    struct inner_t;

private:
    // This pimpl is here to prevent calling some "init" method to safely call shared_from_this to pass to timer.
    std::shared_ptr<inner_t> inner;
};

}} // namespace cocaine::isolate

#endif
