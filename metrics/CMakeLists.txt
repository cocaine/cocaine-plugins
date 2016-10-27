INCLUDE_DIRECTORIES(
        ${PROJECT_SOURCE_DIR}/postgres/include
        ${PROJECT_SOURCE_DIR}/metrics/include
)

LOCATE_LIBRARY(libpqxx "pqxx/version.hxx" "pqxx")

ADD_LIBRARY(metrics-plugin MODULE
        src/api/sender.cpp
        src/sender/postgres.cpp
        src/service/metrics.cpp
        src/module.cpp
        ../postgres/src/pool.cpp
        )

TARGET_LINK_LIBRARIES(metrics-plugin
        metrics
        msgpack
        blackhole
        cocaine-core
        cocaine-io-util
        pqxx)

SET_TARGET_PROPERTIES(metrics-plugin PROPERTIES
        OUTPUT_NAME "metrics"
        PREFIX ""
        SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
        COMPILE_FLAGS "-std=c++0x -Wall -Werror -Wextra -pedantic -Winit-self -Wold-style-cast -Woverloaded-virtual -Wshadow -Wctor-dtor-privacy -Wnon-virtual-dtor")

INSTALL(TARGETS metrics-plugin
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)

INSTALL(
        DIRECTORY
        include/cocaine/idl
        DESTINATION include/cocaine
        COMPONENT development)
