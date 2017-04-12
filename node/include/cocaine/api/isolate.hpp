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

#ifndef COCAINE_ISOLATE_API_HPP
#define COCAINE_ISOLATE_API_HPP

#include <cocaine/common.hpp>

#include <cocaine/locked_ptr.hpp>

#include <map>
#include <memory>

namespace cocaine {
namespace api {

// Cancellation token.
struct cancellation_t {
    virtual
    ~cancellation_t() {}

    virtual
    void
    cancel() noexcept { }
};

// Adapter to cancel shared state.
struct cancellation_wrapper :
    public api::cancellation_t
{
    cancellation_wrapper(std::shared_ptr<cancellation_t> _ptr) :
        ptr(std::move(_ptr))
    {}

    ~cancellation_wrapper() {
        ptr->cancel();
    }

    virtual void
    cancel() noexcept {
        ptr->cancel();
    }

    std::shared_ptr<cancellation_t> ptr;
};

struct spool_handle_base_t {
    virtual
    ~spool_handle_base_t() = default;

    virtual
    void
    on_abort(const std::error_code&, const std::string& msg) = 0;

    virtual
    void
    on_ready() = 0;
};

struct spawn_handle_base_t {
    virtual
    ~spawn_handle_base_t() = default;

    virtual
    void
    on_terminate(const std::error_code&, const std::string& msg) = 0;

    virtual
    void
    on_ready() = 0;

    virtual
    void
    on_data(const std::string& data) = 0;
};

struct metrics_handle_base_t {
    virtual
    ~metrics_handle_base_t() = default;

    virtual
    void
    on_data(const dynamic_t& data) = 0;

    virtual
    void
    on_error(const std::error_code& ec, const std::string& msg) = 0;
};

typedef std::map<std::string, std::string> args_t;
typedef std::map<std::string, std::string> env_t;

struct isolate_t {
    typedef isolate_t category_type;

    virtual
    ~isolate_t() = default;

    virtual
    std::unique_ptr<cancellation_t>
    spool(std::shared_ptr<api::spool_handle_base_t> handler) = 0;

    virtual
    std::unique_ptr<cancellation_t>
    spawn(const std::string& path, const args_t& args, const env_t& environment,
                std::shared_ptr<api::spawn_handle_base_t> handler) = 0;

    virtual
    void
    metrics(const dynamic_t& query,
        std::shared_ptr<api::metrics_handle_base_t> handler) const = 0;

    asio::io_service&
    get_io_service() {
        return io_service;
    }

protected:
    isolate_t(context_t&,
              asio::io_service& io_service,
              const std::string& /* manifest_name */,
              const std::string& /* isolation_type */,
              const dynamic_t& /* args */
    ) :
    io_service(io_service) { }

private:
    asio::io_service& io_service;
};

typedef std::shared_ptr<isolate_t> isolate_ptr;

}
} // namespace cocaine::api

#endif
