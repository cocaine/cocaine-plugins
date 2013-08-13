IF(ELASTICSEARCH)
    INCLUDE_DIRECTORIES(
        ${PROJECT_SOURCE_DIR}/elasticsearch/include)

    LINK_DIRECTORIES(
    )

    ADD_LIBRARY(elasticsearch MODULE
        include/cocaine/io/service/elasticsearch.hpp
        include/cocaine/io/protocol.hpp
        src/elasticsearch
        src/module)

    TARGET_LINK_LIBRARIES(elasticsearch
        swarm
        cocaine-core
    )

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
