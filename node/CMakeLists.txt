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
endif()

include_directories(
    ${PROJECT_SOURCE_DIR}/node/include)

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
)

target_link_libraries(node
    ${Boost_LIBRARIES}
    ${LibArchive_LIBRARIES}
    ${LibCGroup_LIBRARY}
    msgpack
    cocaine-core)

set_target_properties(node PROPERTIES
    PREFIX ""
    SUFFIX ".2.cocaine-plugin"
    COMPILE_FLAGS "-std=c++0x")

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
