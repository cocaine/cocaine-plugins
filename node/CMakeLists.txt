find_package(Boost 1.46 COMPONENTS filesystem system thread REQUIRED)
find_package(LibArchive REQUIRED)

if(NOT APPLE)
    option(COCAINE_ALLOW_CGROUPS "Build CGroups support for Process Isolate" ON)
else()
    option(COCAINE_ALLOW_CGROUPS "Build CGroups support for Process Isolate" OFF)
endif()

if(COCAINE_ALLOW_CGROUPS)
    locate_library(LibCGroup "libcgroup.h" "cgroup")

    set(LibCGroup_LIBRARY "cgroup")
    add_definitions(-DCOCAINE_ALLOW_CGROUPS=1)
else()
    message(STATUS "  Node service will be built without CGroups support")
endif()

include_directories(
    ${PROJECT_SOURCE_DIR}/node/include
)

add_library(node MODULE
    src/isolate/process.cpp
    src/isolate/process/archive.cpp
    src/isolate/process/spooler.cpp
    src/module.cpp
    src/node.cpp
    src/node/app.cpp
    src/node/app/stats.cpp
    src/node/dispatch/client.cpp
    src/node/dispatch/worker.cpp
    src/node/error.cpp
    src/node/manifest.cpp
    src/node/overseer.cpp
    src/node/profile.cpp
    src/node/slave.cpp
    src/node/slave/channel.cpp
    src/node/slave/control.cpp
    src/node/slave/error.cpp
    src/node/slave/fetcher.cpp
    src/node/slave/state/active.cpp
    src/node/slave/state/handshaking.cpp
    src/node/slave/state/sealing.cpp
    src/node/slave/state/spawning.cpp
    src/node/slave/state/state.cpp
    src/node/slave/state/stopped.cpp
    src/node/slave/state/terminating.cpp
    src/stream
)

target_link_libraries(node
    ${Boost_LIBRARIES}
    ${LibArchive_LIBRARIES}
    ${LibCGroup_LIBRARY}
    msgpack
    blackhole
    cocaine-core)

set_target_properties(node PROPERTIES
    PREFIX ""
    SUFFIX ".2${COCAINE_PLUGIN_SUFFIX}"
    COMPILE_FLAGS "-std=c++0x -Wall -Wextra -Winit-self -Wconversion -Wno-sign-conversion -Wold-style-cast -Woverloaded-virtual -Wctor-dtor-privacy -Wnon-virtual-dtor -Wunreachable-code -pedantic -pedantic-errors"
)

install(TARGETS node
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)

install(
    DIRECTORY
        include/cocaine/api
        include/cocaine/idl
        include/cocaine/service
    DESTINATION include/cocaine
    COMPONENT development)

option(WITH_TESTS "Build the library with tests" OFF)

if (WITH_TESTS)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/foreign/modules")

    # Prepare testing infrastructure.
    include(DownloadGoogleTesting)

    download_google_testing()

    include_directories(
        SYSTEM ${GTEST_INCLUDE_DIR}
        SYSTEM ${GMOCK_INCLUDE_DIR}
    )

    link_directories(${GTEST_BINARY_DIR} ${GMOCK_BINARY_DIR})

    include_directories(
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/node/include
        SYSTEM ${CMAKE_SOURCE_DIR}/foreign/asio/asio/include
    )

    set(SOURCES
        tests/slave
    )

    add_executable(node-tests ${SOURCES})
    add_dependencies(node-tests googlemock)
    target_link_libraries(node-tests
        node
        gmock
        gmock_main
        gtest
    )

    # TODO: Turn on all the warnigns while building Node service. There are lot of warnings.
    set_target_properties(node-tests PROPERTIES
        COMPILE_FLAGS "-std=c++0x -Wall -Wextra -Winit-self -Wconversion -Wno-sign-conversion -Wold-style-cast -Woverloaded-virtual -Wctor-dtor-privacy -Wnon-virtual-dtor -Wunreachable-code -pedantic -pedantic-errors")
endif()
