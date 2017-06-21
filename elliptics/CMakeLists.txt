CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(COCAINE-PLUGIN-ELLIPTICS)

IF("${CMAKE_SHARED_LIBRARY_SUFFIX}" STREQUAL "")
    SET(COCAINE_PLUGIN_SUFFIX ".cocaine-plugin")
ELSE()
    SET(COCAINE_PLUGIN_SUFFIX ".cocaine-plugin${CMAKE_SHARED_LIBRARY_SUFFIX}")
ENDIF()

include_directories(
    ${PROJECT_SOURCE_DIR}/foreign/asio/asio/include
    ${PROJECT_SOURCE_DIR}/foreign/blackhole/src
    ${PROJECT_SOURCE_DIR}/include
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
    elliptics_cpp
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
