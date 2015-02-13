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

#ifndef COCAINE_GRAPHITE_METRIC_HPP
#define COCAINE_GRAPHITE_METRIC_HPP

#include <string>
#include <asio/ip/tcp.hpp>
#include <msgpack.hpp>

namespace cocaine { namespace service { namespace graphite {
class metric_t {
public:
    metric_t();

    metric_t(std::string name, double value);

    metric_t(std::string name, double value, time_t ts);

    metric_t(metric_t &&other);

    metric_t(const metric_t& other) = default;

    metric_t &operator=(metric_t&& other);

    metric_t &operator=(const metric_t& other) = default;

    auto get_name() const -> const std::string&;

    auto get_value() const -> double;

    auto get_timestamp() const -> time_t;

    auto empty() const -> bool;

    void msgpack_unpack(const msgpack::object& source);

    auto format(std::string prefix = "") const -> std::string;
private:
    std::string name;
    double value;
    time_t ts;
};

typedef  std::vector<metric_t> metric_pack_t;

}}}
#endif