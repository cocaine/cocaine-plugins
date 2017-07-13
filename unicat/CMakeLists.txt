INCLUDE_DIRECTORIES(
${PROJECT_SOURCE_DIR}/unicat/include
${PROJECT_SOURCE_DIR}/unicorn/include
${PROJECT_SOURCE_DIR}/unicat/src)

ADD_LIBRARY(unicat MODULE
    src/backend/fabric
    src/backend/backend
    src/backend/unicorn
    src/backend/storage
    src/auth_cache
    src/completion
    src/unicat
    src/metainfo
    src/module)

TARGET_LINK_LIBRARIES(unicat
    blackhole
    msgpack
    cocaine-core
    cocaine-io-util
    ${Boost_LIBRARIES})

SET_TARGET_PROPERTIES(unicat PROPERTIES
    PREFIX ""
    SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
    COMPILE_FLAGS "-std=c++11 -O2 -Wall -pedantic -Wnon-virtual-dtor")

INSTALL(TARGETS unicat
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)

INSTALL(
    DIRECTORY include/cocaine/idl
    DESTINATION include/cocaine
    COMPONENT development)
