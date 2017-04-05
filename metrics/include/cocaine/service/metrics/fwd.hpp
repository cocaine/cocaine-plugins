#pragma once

namespace cocaine {
namespace service {

namespace libmetrics = ::metrics;

namespace metrics {

class filter_t;
class getter_t;
class registry_t;

template<typename T>
using node = std::function<T(const libmetrics::tagged_t& metric)>;

} // namespace metrics
} // namespace service
} // namespace cocaine
