INCLUDE_DIRECTORIES(
        ${PROJECT_SOURCE_DIR}/postgres/include)

LOCATE_LIBRARY(libpqxx "pqxx/version.hxx" "pqxx")

ADD_LIBRARY(postgres MODULE
        src/api/postgres/pool.cpp
        src/error/postgres.cpp
        src/module.cpp
        src/pool.cpp
        src/postgres.cpp)

TARGET_LINK_LIBRARIES(postgres
        msgpack
        blackhole
        cocaine-core
        cocaine-io-util
        pqxx)

SET_TARGET_PROPERTIES(postgres PROPERTIES
        PREFIX ""
        SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
        COMPILE_FLAGS "-std=c++0x -Wall -Werror -Wextra -pedantic -Winit-self -Wold-style-cast -Woverloaded-virtual -Wshadow -Wctor-dtor-privacy -Wnon-virtual-dtor")

INSTALL(TARGETS postgres
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)

INSTALL(
        DIRECTORY
        include/cocaine/error
        DESTINATION include/cocaine
        COMPONENT development)
