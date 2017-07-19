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

#include "cocaine/zookeeper.hpp"
#include "cocaine/traits/unicorn.hpp"

#include <boost/algorithm/string/find.hpp>

#include <cocaine/dynamic.hpp>
#include <cocaine/errors.hpp>

#include <iostream>

namespace cocaine {
namespace zookeeper {

auto path_parent(const path_t& path, unsigned int depth) -> path_t {
    if(path.empty()) {
        throw error_t("invalid path for path parent - {}", path);
    }
    if(depth == 0) {
        return path;
    }
    auto it = boost::find_nth(path, "/", -depth);
    if(it.empty()) {
        throw error_t("could not get {} level parent from path: {}", depth, path);
    }
    return path.substr(0, it.begin() - path.begin());
}

auto is_valid_sequence_node(const path_t& path) -> bool {
    static unsigned int zoo_sequence_length = 10;
    return path.size() - path.find_last_not_of("0123456789") - 1 == zoo_sequence_length;
}

auto get_node_name(const path_t& path) -> std::string {
    auto pos = path.find_last_of('/');
    if(pos == std::string::npos || pos == path.size()-1) {
        throw error_t(cocaine::error::invalid_path, "invalid path specified - {}", path);
    }
    return path.substr(pos+1);
}

auto serialize(const unicorn::value_t& val) -> std::string {
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> packer(buffer);
    cocaine::io::type_traits<cocaine::dynamic_t>::pack(packer, val);
    return std::string(buffer.data(), buffer.size());
}

auto unserialize(const std::string& val) -> unicorn::value_t {
    msgpack::object obj;
    std::unique_ptr<msgpack::zone> z(new msgpack::zone());

    msgpack_unpack_return ret = msgpack_unpack(
            val.c_str(), val.size(), nullptr, z.get(),
            reinterpret_cast<msgpack_object*>(&obj)
    );

    //Only strict unparse.
    if(static_cast<msgpack::unpack_return>(ret) != msgpack::UNPACK_SUCCESS) {
        throw std::system_error(cocaine::error::unicorn_errors::invalid_value);
    }
    unicorn::value_t target;
    cocaine::io::type_traits<cocaine::dynamic_t>::unpack(obj, target);
    return target;
}

auto map_zoo_error(int rc) -> std::error_code {
    if(rc == ZCONNECTIONLOSS || rc == ZSESSIONEXPIRED || rc == ZCLOSING) {
        return make_error_code(error::unicorn_errors::connection_loss);
    }
    if(rc == ZNONODE) {
        return make_error_code(error::unicorn_errors::no_node);
    }
    if(rc == ZNODEEXISTS) {
        return make_error_code(error::unicorn_errors::node_exists);
    }
    if(rc == ZBADVERSION) {
        return make_error_code(error::unicorn_errors::version_not_allowed);
    }
    return make_error_code(error::unicorn_errors::backend_internal_error);
}

auto event_to_string(int event) -> std::string {
    if(event == ZOO_CREATED_EVENT) {
        return "created";
    } else if(event == ZOO_DELETED_EVENT) {
        return "deleted";
    } else if(event == ZOO_CHANGED_EVENT) {
        return "changed";
    } else if(event == ZOO_CHILD_EVENT) {
        return "child";
    } else if(event == ZOO_SESSION_EVENT) {
        return "session";
    } else if(event == ZOO_NOTWATCHING_EVENT) {
        return "not watching";
    }
    return "unknown";
}

auto state_to_string(int state) -> std::string {
    if(state == ZOO_EXPIRED_SESSION_STATE) {
        return "expired session";
    } else if(state == ZOO_AUTH_FAILED_STATE) {
        return "auth failed";
    } else if(state == ZOO_CONNECTING_STATE) {
        return "connecting";
    } else if(state == ZOO_ASSOCIATING_STATE) {
        return "associating";
    } else if(state == ZOO_CONNECTED_STATE) {
        return "connected";
    }
    return "unknown";
}

} // namespace zookeeper
} // namespace cocaine
