#pragma once

#include <deque>
#include <unordered_map>

#include <metrics/fwd.hpp>
#include <metrics/usts/ewma.hpp>

#include <cocaine/locked_ptr.hpp>

#include "cocaine/idl/node.hpp"

#include "cocaine/detail/service/node/slave.hpp"
#include "cocaine/detail/service/node/slave/load.hpp"
#include "cocaine/detail/service/node/stats.hpp"

namespace cocaine {
namespace service {
namespace node {
namespace info {

using cocaine::detail::service::node::slave_t;
using cocaine::detail::service::node::slave::load_t;

typedef std::deque<load_t> queue_type;
typedef std::unordered_map<std::string, slave_t> pool_type;

// Helper tagged struct.
struct queue_t {
    unsigned long capacity;

    const synchronized<queue_type>* queue;
    metrics::usts::ewma_t& queue_depth;
};

// Helper tagged struct.
struct pool_t {
    unsigned long capacity;

    std::int64_t spawned;
    std::int64_t crashed;

    const synchronized<pool_type>* pool;
};

class info_collector_t {
    cocaine::io::node::info::flags_t flags;
    dynamic_t::object_t& result;

public:
    info_collector_t(cocaine::io::node::info::flags_t flags, dynamic_t::object_t* result);

    // Incoming requests.
    void visit(std::int64_t accepted, std::int64_t rejected) const;

    // Pending events queue.
    void visit(const queue_t& value);

    void visit(metrics::meter_t& meter);

    template<typename Accumulate>
    void visit(metrics::timer<Accumulate>& timer);

    void visit(const pool_t& value);
};

} // namespace info
} // namespace node
} // namespace service
} // namespace cocaine
