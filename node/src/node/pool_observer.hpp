#pragma once

namespace cocaine {
namespace service {
namespace node {

class pool_observer {
public:
    virtual ~pool_observer() {}
    virtual auto spawned() -> void = 0;
    virtual auto despawned() -> void = 0;
};

}
}
}
