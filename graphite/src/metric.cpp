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

#include <cocaine/metric.hpp>
#include <cocaine/traits/vector.hpp>

namespace cocaine { namespace service { namespace graphite {

metric_t::metric_t() :
    name(),
    value(),
    ts()
{
}

metric_t::metric_t(std::string _name, double _value) :
    name(std::move(_name)),
    value(_value),
    ts(time(nullptr))
{
}

metric_t::metric_t(std::string _name, double _value, time_t _ts) :
    name(std::move(_name)),
    value(_value),
    ts(_ts)
{
}

metric_t::metric_t(metric_t&& other) :
    name(std::move(other.name)),
    value(other.value),
    ts(other.ts)
{
}

metric_t& metric_t::operator=(metric_t&& other) {
    name = std::move(other.name);
    value = other.value;
    ts = other.ts;
    return *this;
}

std::string const& metric_t::get_name() const {
    return name;
}

double metric_t::get_value() const {
    return value;
}

time_t metric_t::get_timestamp() const {
    return ts;
}

bool metric_t::empty() const {
    return name.empty();
}

void metric_t::msgpack_unpack(const msgpack::object& source) {
    if(source.type != msgpack::type::ARRAY) {
        throw msgpack::type_error();
    }
    auto ar = source.via.array;
    if(ar.size != 2 && ar.size != 3) {
        msgpack::unpack_error("Invalid data size to unpack graphite::metrics_t");
    }
    name = std::move(ar.ptr[0].as<std::string>());
    value = ar.ptr[1].as<double>();
    ts = ar.size == 3 ? ar.ptr[2].as<time_t>() : time(nullptr);
}

std::string metric_t::format() const {
    return
        get_name() + ' ' +
        std::to_string(get_value()) + ' ' +
        std::to_string(get_timestamp()) + '\n';
}
}}}