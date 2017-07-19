set(PLUGIN_NAME node)

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
    set(CGROUP_SOURCES
            src/isolate/process/cgroup.cpp
    )
else()
    message(STATUS "  Node service will be built without CGroups support")
    set(CGROUP_SOURCES
            src/isolate/process/cgroup_stub.cpp
    )
endif()

include_directories(
    ${PROJECT_SOURCE_DIR}/node/include
    ${PROJECT_SOURCE_DIR}/node/src
)

add_library(${PLUGIN_NAME} SHARED
    src/isolate/process.cpp
    src/isolate/process/archive.cpp
    ${CGROUP_SOURCES}
    src/isolate/external.cpp
    src/isolate/fetcher.cpp

    src/node/info/manifest.cpp
    src/node/info/profile.cpp
    src/node/info/collector.cpp

    src/module.cpp
    src/node.cpp
    src/node/app.cpp
    src/node/dispatch/client.cpp
    src/node/dispatch/worker.cpp
    src/node/engine.cpp
    src/node/isometrics.cpp
    src/node/error.cpp
    src/node/manifest.cpp
    src/node/overseer.cpp
    src/node/profile.cpp
    src/node/slave.cpp
    src/node/slave/channel.cpp
    src/node/slave/control.cpp
    src/node/slave/error.cpp
    src/node/slave/id.cpp
    src/node/slave/machine.cpp
    src/node/slave/state/active.cpp
    src/node/slave/state/handshaking.cpp
    src/node/slave/state/inactive.cpp
    src/node/slave/state/preparation.cpp
    src/node/slave/state/seal.cpp
    src/node/slave/state/spawn.cpp
    src/node/slave/state/state.cpp
    src/node/slave/state/terminate.cpp
    src/node/slave/stats.cpp
    src/node/stats.cpp
    src/stream.cpp
    src/node/slave/spawn_handle.cpp
)

add_library(isolate_mock SHARED
    tests/isolate_mock.cpp
)

target_link_libraries(isolate_mock
	${PLUGIN_NAME}
)

target_link_libraries(${PLUGIN_NAME}
    ${Boost_LIBRARIES}
    ${LibArchive_LIBRARIES}
    ${LibCGroup_LIBRARY}
    msgpack
    blackhole
    metrics
    cocaine-core
    cocaine-io-util)

set(BUILD_FLAGS "-std=c++0x")
set(BUILD_FLAGS "${BUILD_FLAGS} -Wall")
set(BUILD_FLAGS "${BUILD_FLAGS} -Wextra")
set(BUILD_FLAGS "${BUILD_FLAGS} -Waddress")
set(BUILD_FLAGS "${BUILD_FLAGS} -Warray-bounds")
set(BUILD_FLAGS "${BUILD_FLAGS} -Wbuiltin-macro-redefined")
set(BUILD_FLAGS "${BUILD_FLAGS} -Wctor-dtor-privacy")
set(BUILD_FLAGS "${BUILD_FLAGS} -Winit-self")
set(BUILD_FLAGS "${BUILD_FLAGS} -Wnon-virtual-dtor")
set(BUILD_FLAGS "${BUILD_FLAGS} -Wold-style-cast")
set(BUILD_FLAGS "${BUILD_FLAGS} -Woverloaded-virtual")
set(BUILD_FLAGS "${BUILD_FLAGS} -Wswitch")
set(BUILD_FLAGS "${BUILD_FLAGS} -Wunreachable-code")
set(BUILD_FLAGS "${BUILD_FLAGS} -pedantic -pedantic-errors")

set_target_properties(${PLUGIN_NAME} PROPERTIES
    COMPILE_FLAGS "${BUILD_FLAGS}"
    PREFIX ""
    SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
)

set_target_properties(isolate_mock PROPERTIES
    COMPILE_FLAGS "${BUILD_FLAGS}"
    PREFIX ""
    SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
)

install(TARGETS ${PLUGIN_NAME}
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)

install(
    DIRECTORY
        include/cocaine/api
        include/cocaine/idl
        include/cocaine/repository
        include/cocaine/service
    DESTINATION include/cocaine
    COMPONENT development)
