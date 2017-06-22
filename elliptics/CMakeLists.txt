CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

include_directories(
    ${PROJECT_SOURCE_DIR}/foreign/asio/asio/include
    ${PROJECT_SOURCE_DIR}/elliptics/include
)

ADD_LIBRARY(elliptics-extensions MODULE
    src/service
    src/storage
    src/module)

TARGET_LINK_LIBRARIES(elliptics-extensions
    blackhole
    cocaine-core
    cocaine-io-util
    elliptics_client
    ${Boost_LIBRARIES})

SET_TARGET_PROPERTIES(elliptics-extensions PROPERTIES
    PREFIX ""
    SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
    COMPILE_FLAGS "-std=c++0x")

INSTALL(TARGETS elliptics-extensions
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)

INSTALL(
    DIRECTORY
        include/cocaine/
    DESTINATION include/cocaine
    COMPONENT development)
