INCLUDE_DIRECTORIES(
    ${PROJECT_SOURCE_DIR}/cache/include)

ADD_LIBRARY(cache MODULE
    src/cache
    src/module)

TARGET_LINK_LIBRARIES(cache
    cocaine-core
    blackhole
    ${Boost_LIBRARIES})

SET_TARGET_PROPERTIES(cache PROPERTIES
    PREFIX ""
    SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
    COMPILE_FLAGS "-std=c++0x")

INSTALL(TARGETS cache
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)

INSTALL(
    DIRECTORY
        include/cocaine/idl
    DESTINATION include/cocaine
    COMPONENT development)
