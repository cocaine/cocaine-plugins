INCLUDE_DIRECTORIES(
        ${PROJECT_SOURCE_DIR}/vicodyn/include)

ADD_LIBRARY(vicodyn SHARED
        src/api/vicodyn/balancer.cpp
        src/gateway/vicodyn.cpp
        src/module.cpp
        src/vicodyn/balancer/simple.cpp
        src/vicodyn/debug.cpp
        src/vicodyn/pool.cpp
        src/vicodyn/proxy.cpp
        src/vicodyn/proxy/dispatch.cpp
        src/vicodyn/peer.cpp
        src/vicodyn/queue/invocation.cpp
        src/vicodyn/session.cpp
        src/vicodyn/stream.cpp
        )

ADD_DEFINITIONS("-DVICODYN_USE_DEBUG")
TARGET_LINK_LIBRARIES(vicodyn
        msgpack
        blackhole
        cocaine-core
        cocaine-io-util
        metrics
        ${Boost_LIBRARIES})

SET_TARGET_PROPERTIES(vicodyn PROPERTIES
        PREFIX ""
        SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
        COMPILE_FLAGS "-std=c++0x -Wall -Wextra -pedantic -Winit-self -Wold-style-cast -Woverloaded-virtual -Wctor-dtor-privacy -Wnon-virtual-dtor")

INSTALL(TARGETS vicodyn
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)

INSTALL(DIRECTORY
        include/cocaine
        DESTINATION include
        COMPONENT development)
