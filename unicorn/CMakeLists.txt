INCLUDE_DIRECTORIES(
    ${PROJECT_SOURCE_DIR}/unicorn/include)

ADD_LIBRARY(unicorn MODULE
    include/cocaine/unicorn.hpp
    include/cocaine/zookeeper/session
    include/cocaine/zookeeper/connection
    include/cocaine/zookeeper/handler
    include/cocaine/zookeeper/exception
    include/cocaine/unicorn/path
    include/cocaine/unicorn/value
    include/cocaine/unicorn/handlers

    src/module
    src/unicorn.cpp
    src/unicorn/value
    src/zookeeper/zookeeper
    src/zookeeper/exception
    src/zookeeper/session
    src/zookeeper/handler
    src/zookeeper/connection)

TARGET_LINK_LIBRARIES(unicorn
    msgpack
    cocaine-core
    zookeeper_mt
    ${Boost_LIBRARIES})

SET_TARGET_PROPERTIES(unicorn PROPERTIES
    PREFIX ""
    SUFFIX ".cocaine-plugin"
    COMPILE_FLAGS "-std=c++0x")

INSTALL(TARGETS unicorn
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)