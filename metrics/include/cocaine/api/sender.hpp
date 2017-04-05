/*
    Copyright (c) 2016+ Anton Matveenko <antmat@me.com>
    Copyright (c) 2016+ Other contributors as noted in the AUTHORS file.

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

#include <cocaine/dynamic.hpp>
#include <cocaine/forwards.hpp>

#include <functional>

namespace cocaine {
namespace api {

struct sender_t {
    struct data_provider_t {
        virtual ~data_provider_t() {}
        virtual auto fetch() -> dynamic_t = 0;
    };

    struct function_data_provider_t : public data_provider_t {
        typedef std::function<dynamic_t()> callback_t;

        function_data_provider_t(callback_t _cb) : cb(std::move(_cb)) {}
        virtual auto fetch() -> dynamic_t { return cb(); }

    private:
        callback_t cb;
    };

    typedef sender_t category_type;
    typedef std::unique_ptr<data_provider_t> data_provider_ptr;

    virtual ~sender_t() {
        // Empty.
    }

protected:
    sender_t(context_t& /* context */,
             asio::io_service& /* io_service */,
             const std::string& /* name */,
             data_provider_ptr /* data_provider */,
             const dynamic_t& /* args */) {}

};

typedef std::unique_ptr<sender_t> sender_ptr;

sender_ptr sender(context_t& context,
                  asio::io_service& io_service,
                  const std::string& name,
                  sender_t::data_provider_ptr data_provider);

}
} // namespace cocaine::api

