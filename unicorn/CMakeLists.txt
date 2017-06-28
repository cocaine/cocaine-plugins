INCLUDE_DIRECTORIES(
    ${PROJECT_SOURCE_DIR}/foreign/asio
    ${PROJECT_SOURCE_DIR}/unicorn/include
    ${PROJECT_SOURCE_DIR}/unicorn/src)

LOCATE_LIBRARY(LIBZOOKEEPER_MT "zookeeper/zookeeper.h" "zookeeper_mt")

ADD_LIBRARY(unicorn MODULE
    src/cluster/unicorn.cpp
    src/authorization/unicorn.cpp
    src/module.cpp
    src/repository/zookeeper.cpp
    src/service/unicorn.cpp
    src/unicorn/zookeeper.cpp
    src/zookeeper.cpp
    src/zookeeper/connection.cpp
    src/zookeeper/session.cpp
)

TARGET_LINK_LIBRARIES(unicorn
    msgpack
    blackhole
    cocaine-core
    cocaine-io-util
    zookeeper_mt
    ${Boost_LIBRARIES})

SET_TARGET_PROPERTIES(unicorn PROPERTIES
    PREFIX ""
    SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
    COMPILE_FLAGS "-std=c++0x -Wall -Werror -Wextra -pedantic -Winit-self -Wold-style-cast -Woverloaded-virtual -Wctor-dtor-privacy -Wnon-virtual-dtor")

INSTALL(TARGETS unicorn
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)

INSTALL(
    DIRECTORY
        include/cocaine/idl
    DESTINATION include/cocaine
    COMPONENT development)
