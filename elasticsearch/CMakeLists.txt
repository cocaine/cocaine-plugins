OPTION(ELASTICSEARCH_DELETE_SUPPORT "Enable 'delete' method support" OFF)
OPTION(ELASTICSEARCH_PLUGIN_TESTING "Enable plugin testing" OFF)

CONFIGURE_FILE(
    "${PROJECT_SOURCE_DIR}/elasticsearch/config.hpp.in"
    "${PROJECT_SOURCE_DIR}/elasticsearch/include/cocaine/service/elasticsearch/config.hpp")

INCLUDE_DIRECTORIES(
    ${PROJECT_SOURCE_DIR}/elasticsearch/include)

LINK_DIRECTORIES()

ADD_LIBRARY(elasticsearch MODULE
    src/elasticsearch
    src/module)

TARGET_LINK_LIBRARIES(elasticsearch
    blackhole
    swarm
    swarm_urlfetcher
    cocaine-core
    cocaine-io-util)

SET_TARGET_PROPERTIES(elasticsearch PROPERTIES
    PREFIX ""
    SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
    COMPILE_FLAGS "-std=c++0x")

INSTALL(TARGETS elasticsearch
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)

INSTALL(
    DIRECTORY
        include/cocaine/idl
    DESTINATION include/cocaine
    COMPONENT development)

ADD_SUBDIRECTORY(tests)
