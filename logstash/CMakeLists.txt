IF(LOGSTASH)
    INCLUDE_DIRECTORIES(
        ${PROJECT_SOURCE_DIR}/logstash/include)

    LINK_DIRECTORIES(
        )

    ADD_LIBRARY(logstash MODULE
        src/logstash
        src/module)

    TARGET_LINK_LIBRARIES(logstash
        cocaine-core
        boost_system-mt)

    SET_TARGET_PROPERTIES(logstash PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin"
        COMPILE_FLAGS "-std=c++0x")

    INSTALL(TARGETS logstash
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)

    INSTALL(
        DIRECTORY
            include/
        DESTINATION include
        COMPONENT development)
ENDIF(LOGSTASH)
