INCLUDE_DIRECTORIES(
    ${PROJECT_SOURCE_DIR}/graphite/include)

ADD_LIBRARY(graphite MODULE
    include/cocaine/graphite
    include/cocaine/metric
    include/cocaine/idl/graphite
    src/module
    src/metric
    src/graphite)

TARGET_LINK_LIBRARIES(graphite
    blackhole
    cocaine-core
    cocaine-io-util
    ${Boost_LIBRARIES})

SET_TARGET_PROPERTIES(graphite PROPERTIES
    PREFIX ""
    SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
    COMPILE_FLAGS "-std=c++0x")

INSTALL(TARGETS graphite
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)

INSTALL(
    DIRECTORY
        include/cocaine/idl
    DESTINATION include/cocaine
    COMPONENT development)
