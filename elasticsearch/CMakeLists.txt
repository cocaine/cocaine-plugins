IF(ELASTICSEARCH)
    OPTION(ELASTICSEARCH_DELETE_SUPPORT "Enable 'delete' method support" OFF)

    CONFIGURE_FILE(
        "${PROJECT_SOURCE_DIR}/elasticsearch/config.hpp.in"
        "${PROJECT_SOURCE_DIR}/elasticsearch/include/cocaine/service/elasticsearch/config.hpp")

    INCLUDE_DIRECTORIES(
        ${PROJECT_SOURCE_DIR}/elasticsearch/include)

    LINK_DIRECTORIES()

    ADD_LIBRARY(elasticsearch MODULE
        include/cocaine/service/elasticsearch.hpp
        include/cocaine/io/protocol.hpp
        src/elasticsearch
        src/rest/handlers.hpp
        src/rest/get.hpp
        src/rest/get.cpp
        src/rest/index.hpp
        src/rest/index.cpp
        src/rest/search.hpp
        src/rest/search.cpp
        src/rest/delete.hpp
        src/rest/delete.cpp
        src/module)

    TARGET_LINK_LIBRARIES(elasticsearch
        swarm
        cocaine-core)

    SET_TARGET_PROPERTIES(elasticsearch PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin"
        COMPILE_FLAGS "-std=c++0x")

    INSTALL(TARGETS elasticsearch
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)

    INSTALL(
        DIRECTORY
            include/
        DESTINATION include
        COMPONENT development)
ENDIF(ELASTICSEARCH)
