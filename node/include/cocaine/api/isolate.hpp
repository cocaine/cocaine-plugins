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
#include <cocaine/repository.hpp>

#include <mutex>

#include <asio/io_service.hpp>

namespace cocaine { namespace api {

struct handle_t {
    virtual
   ~handle_t() {
        // Empty.
    }

    virtual
    void
    terminate() = 0;

    virtual
    int
    stdout() const = 0;
};

typedef std::map<std::string, std::string> string_map_t;

// Cancellation token.
struct cancellation_t {
    virtual
   ~cancellation_t() {
        // Empty.
    }

    virtual
    void
    cancel() {}
};

struct isolate_t {
    typedef isolate_t category_type;

    virtual
   ~isolate_t() {
        // Empty.
    }

    virtual
    void
    spool() = 0;

    // Default implementation delegates the control flow into the blocking spool method.
    virtual
    std::unique_ptr<cancellation_t>
    async_spool(std::function<void(const std::error_code&)> handler) {
        spool();
        get_io_service().post(std::bind(handler, std::error_code()));
        return std::make_unique<cancellation_t>();
    }

    virtual
    std::unique_ptr<handle_t>
    spawn(const std::string& path, const string_map_t& args, const string_map_t& environment) = 0;

    asio::io_service&
    get_io_service() {
        return io_service;
    }

protected:
    isolate_t(context_t&, asio::io_service& io_service, const std::string&, const dynamic_t& /* args */):
        io_service(io_service)
    {}

private:
    asio::io_service& io_service;
};

template<>
struct category_traits<isolate_t> {
    typedef std::shared_ptr<isolate_t> ptr_type;

    struct factory_type: public basic_factory<isolate_t> {
        virtual
        ptr_type
        get(context_t& context, asio::io_service& io_context, const std::string& name, const dynamic_t& args) = 0;
    };

    template<class T>
    struct default_factory: public factory_type {
        virtual
        ptr_type
        get(context_t& context, asio::io_service& io_context, const std::string& name, const dynamic_t& args) {
            ptr_type instance;

            instances.apply([&](std::map<std::string, std::weak_ptr<isolate_t>>& instances) {
                auto weak_ptr = instances[name];

                if((instance = weak_ptr.lock()) == nullptr) {
                    instance = std::make_shared<T>(context, io_context, name, args);
                    instances[name] = instance;
                }
            });

            return instance;
        }

    private:
        synchronized<std::map<std::string, std::weak_ptr<isolate_t>>> instances;
    };
};

}} // namespace cocaine::api

#endif
