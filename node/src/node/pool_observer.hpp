#pragma once

namespace cocaine {
namespace service {
namespace node {

class pool_observer {
public:
    virtual ~pool_observer() = default;

    /// Called when a slave was spawned.
    virtual
    auto
    spawned() -> void = 0;

    /// Called when a slave was despawned.
    virtual
    auto
    despawned() -> void = 0;
};

} // namespace node
} // namespace service
} // namespace cocaine
