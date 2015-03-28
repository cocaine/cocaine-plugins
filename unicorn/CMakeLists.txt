INCLUDE_DIRECTORIES(
    ${PROJECT_SOURCE_DIR}/unicorn/include)

ADD_LIBRARY(unicorn MODULE
    src/module.cpp
    src/cluster/unicorn.cpp
    src/unicorn/api/zookeeper.cpp
    src/unicorn.cpp
    src/unicorn/value.cpp
    src/zookeeper/zookeeper.cpp
    src/zookeeper/exception.cpp
    src/zookeeper/session.cpp
    src/zookeeper/handler.cpp
    src/zookeeper/connection.cpp
)

TARGET_LINK_LIBRARIES(unicorn
    msgpack
    cocaine-core
    zookeeper_mt
    ${Boost_LIBRARIES})

SET_TARGET_PROPERTIES(unicorn PROPERTIES
    PREFIX ""
    SUFFIX ".cocaine-plugin"
    COMPILE_FLAGS "-std=c++0x -Wall -Werror -Wextra -pedantic -Winit-self -Wold-style-cast -Woverloaded-virtual -Wshadow -Wctor-dtor-privacy -Wnon-virtual-dtor")

INSTALL(TARGETS unicorn
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)
