INCLUDE_DIRECTORIES(
    ${PROJECT_SOURCE_DIR}/chrono/include)

ADD_LIBRARY(chrono MODULE
    src/chrono
    src/module)

TARGET_LINK_LIBRARIES(chrono
    cocaine-core
    cocaine-io-util
    blackhole
    ${Boost_LIBRARIES})

SET_TARGET_PROPERTIES(chrono PROPERTIES
    PREFIX ""
    SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
    COMPILE_FLAGS "-std=c++0x")

INSTALL(TARGETS chrono
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)

INSTALL(
    DIRECTORY
        include/cocaine/idl
    DESTINATION include/cocaine
    COMPONENT development)
